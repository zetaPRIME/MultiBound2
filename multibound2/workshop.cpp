#include "util.h" // part of utility header

#include "data/config.h"
#include "data/instance.h"

#include <deque>

#include <QSet>
#include <QHash>

#include <QJsonDocument>
#include <QJsonArray>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>

#include <QWebEnginePage>
#include <QWebEngineSettings>

namespace { // utilities
    QString jQuery;
}

void MultiBound::Util::updateFromWorkshop(MultiBound::Instance* inst, bool save) {
    updateStatus(qs("Fetching collection details..."));

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
        jQuery = get(qs("https://code.jquery.com/jquery-3.4.1.slim.min.js"))->readAll();

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

        // request synchronously
        page->load(workshopLinkFromId(cId));
        ev.exec();
        // inject jQuery; we don't need to do this synchronously, it'll just batch with the first request
        page->runJavaScript(jQuery);

        if (needsInfo) { // update display name and title from html
            needsInfo = false;

            auto name = queryPage("$('div.collectionHeaderContent div.workshopItemTitle').first().text()").toString();
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

        // and queue up subcollections
        {
            QString q = " \
                (function(){ var res = [ ]; \
                $('div.collectionChildren div.collections > div[class^=\\'workshopItem\\'').each(function(){ \
                res.push( $(this).find('a').attr('href') ); \
                }); \
                return res; })();";
            auto colls = queryPage(q).toJsonArray();
            for (auto cn : colls) {
                auto id = workshopIdFromLink(cn.toString());
                if (!id.isEmpty()) wsQueue.push_back(id);
            }
        }

        if (needsExtCfg) { // find extcfg block
            needsExtCfg = false;

            json["extCfg"] = queryPage("eval('json_out = ' + $('div.workshopItemDescriptionForCollection div.bb_code').last().html().replace(/<br>/gi,'\\n') + '; json_out')").toJsonObject();
        }

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

    if (Config::steamcmdEnabled) Util::updateMods(inst);
    updateStatus("");
}
