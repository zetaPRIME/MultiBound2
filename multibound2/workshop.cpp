#include "util.h" // part of utility header

#include "data/instance.h"

#include <deque>

#include <QSet>
#include <QHash>

#include <QJsonDocument>
#include <QJsonArray>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>

/*
#include <libxml/tree.h>
#include <libxml/HTMLparser.h>
#include <libxml++/libxml++.h>
*/

#include <QWebEnginePage>
#include <QWebEngineSettings>

namespace { // utilities
    /*inline QString str(xmlpp::Node* n) {
        if (auto nn = dynamic_cast<xmlpp::ContentNode*>(n); nn) return QString::fromUtf8(nn->get_content().data());
        if (auto nn = dynamic_cast<xmlpp::AttributeNode*>(n); nn) return QString::fromUtf8(nn->get_value().data());
        return qs();
    }*/

    QString jQuery;
}

void MultiBound::Util::updateFromWorkshop(MultiBound::Instance* inst, bool save) {
    auto wsId = inst->workshopId();
    if (wsId.isEmpty()) return; // no workshop id attached

    auto& json = inst->json;

    auto info = json["info"].toObject();
    info["workshopId"] = wsId; // make sure id tag is present

    QHash<QString, QString> wsMods;
    std::deque<QString> wsQueue;
    QSet<QString> wsVisited;

    QNetworkAccessManager net;
    QEventLoop ev;
    auto get = [&](QUrl url) {
        auto reply = net.get(QNetworkRequest(url));
        QObject::connect(reply, &QNetworkReply::finished, &ev, &QEventLoop::quit);
        ev.exec(); // wait for things
        reply->deleteLater(); // queue deletion on event loop, *after we're done*
        return reply;
    };

    auto page = new QWebEnginePage();
    page->settings()->setAttribute(QWebEngineSettings::AutoLoadImages, false);
    QObject::connect(page, &QWebEnginePage::loadFinished, &ev, &QEventLoop::quit);

    if (jQuery.isEmpty())
        jQuery = get(qs("https://code.jquery.com/jquery-3.4.1.min.js"))->readAll();

    auto queryPage = [&](const QString& q) {
        QVariant res;
        page->runJavaScript(q, [&](const QVariant& r) {
            res = r;
            ev.quit();
        });
        ev.exec();
        return res;
    };

    bool needsInfo = !info["lockInfo"].toBool();
    bool needsExtCfg = true;

    wsQueue.push_back(wsId); // it starts with one
    while (!wsQueue.empty()) {
        QString cId = wsQueue.front();
        wsQueue.pop_front();
        if (cId.isEmpty() || wsVisited.contains(cId)) continue;
        wsVisited.insert(cId);

        // request synchronously and feed into parser
        /*auto reply = get(workshopLinkFromId(cId));
        if (reply->error() != QNetworkReply::NoError) continue; // failed
        auto replyContents = reply->readAll();
        auto doc = htmlReadDoc(reinterpret_cast<xmlChar*>(replyContents.data()), nullptr, nullptr, HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
        auto root = std::make_shared<xmlpp::Element>(xmlDocGetRootElement(doc));*/

        // request synchronously
        page->load(workshopLinkFromId(cId));
        ev.exec();
        queryPage(jQuery);

        if (needsInfo) { // update display name and title from html
            needsInfo = false;

            /*auto ele = root->find("//div[@class=\"collectionHeaderContent\"]//div[@class=\"workshopItemTitle\"]/text()");
            if (ele.empty()) { // not a collection, abort
                xmlFreeDoc(doc);
                return;
            }*/
            auto name = //str(ele[0]);
                queryPage("$('div.collectionHeaderContent div.workshopItemTitle').first().text()").toString();
            info["name"] = name;
            info["windowTitle"] = qs("Starbound - %1").arg(name);
        }

        // handle mod entries
        {
            QString q = " \
                (function(){ var res = [ ]; \
                $('div.collectionChildren div.collectionItemDetails').each(function(){ \
                res.push({ link : $(this).find('a').first().attr('href'), title : $(this).find('div.workshopItemTitle').first().text() }); \
                }); \
                return res; })();";
            auto mods = queryPage(q).toJsonArray();
            for (auto mn : mods) {
                auto id = workshopIdFromLink(mn.toObject()["link"].toString());
                if (!wsMods.contains(id)) wsMods[id] = mn.toObject()["title"].toString();
            }
        }
        /*auto modNodes = root->find("//div[@class=\"collectionChildren\"]//div[@class=\"collectionItemDetails\"]");
        for (auto mn : modNodes) {
            auto id = workshopIdFromLink(str(mn->find(".//a/attribute::href")[0]));
            if (!wsMods.contains(id)) wsMods[id] = str(mn->find(".//div[@class=\"workshopItemTitle\"]/text()")[0]);
        }*/

        // and queue up subcollections
        {
            QString q = " \
                (function(){ var res = [ ]; \
                $('div.collectionChildren div.workshopBrowseRow > div[class^=\\'workshopItem\\'').each(function(){ \
                res.push( $(this).find('a').attr('href') ); \
                }); \
                return res; })();";
            auto colls = queryPage(q).toJsonArray();
            for (auto cn : colls) {
                auto id = workshopIdFromLink(cn.toString());
                if (!id.isEmpty()) wsQueue.push_back(id);
            }
        }
        /*auto collNodes = root->find("//div[@class=\"collectionChildren\"]//div[@class=\"workshopBrowseRow\"]/div[starts-with(@class, 'workshopItem')]");
        for (auto cn : collNodes) {
            auto id = workshopIdFromLink(str(cn->find(".//a/attribute::href")[0]));
            if (!id.isEmpty()) wsQueue.push_back(id);
        }*/

        if (needsExtCfg) { // find extcfg block
            needsExtCfg = false;

            json["extCfg"] = queryPage("eval('json_out = ' + $('div.workshopItemDescriptionForCollection div.bb_code').last().html().replace(/<br>/gi,'\\n') + '; json_out')").toJsonObject();

            /*auto ele = root->find("//div[@class=\"workshopItemDescriptionForCollection\"]//div[@class=\"bb_code\"][last()]/text()");
            if (!ele.empty()) {
                QString xcs; // demangle the text from workshop's hackiness
                for (auto e : ele) xcs.append(str(e)).append("\n");
                json["extCfg"] = parseJson(xcs.toUtf8()).object();
            }*/
        }

        //xmlFreeDoc(doc);
        page->deleteLater();
    }

    json["info"] = info; // update info in json body

    // rebuild assetSources with new contents
    auto oldSrc = json["assetSources"].toArray();
    QJsonArray src;

    // carry over old entries that aren't workshopAuto
    for (auto s : oldSrc) if (s.isString() || s.toObject()["type"].toString() != "workshopAuto") src << s;

    // and add mod entries
    for (auto i = wsMods.keyValueBegin(); i != wsMods.keyValueEnd(); i++) {
        QJsonObject s;
        s["type"] = "workshopAuto";
        s["id"] = (*i).first;
        s["friendlyName"] = (*i).second;
        src << s;
    }

    json["assetSources"] = src;

    // commit
    if (save) inst->save();
}
