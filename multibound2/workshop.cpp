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

#include <libxml/tree.h>
#include <libxml/HTMLparser.h>
#include <libxml++/libxml++.h>

namespace { // utilities
    inline QString str(xmlpp::Node* n) {
        if (auto nn = dynamic_cast<xmlpp::ContentNode*>(n); nn) return QString::fromUtf8(nn->get_content().data());
        if (auto nn = dynamic_cast<xmlpp::AttributeNode*>(n); nn) return QString::fromUtf8(nn->get_value().data());
        return qs();
    }
}

void MultiBound::Util::updateFromWorkshop(MultiBound::Instance* inst) {
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

    bool needsInfo = !info["lockInfo"].toBool();
    bool needsExtCfg = true;

    wsQueue.push_back(wsId); // it starts with one
    while (!wsQueue.empty()) {
        QString cId = wsQueue.front();
        wsQueue.pop_front();
        if (cId.isEmpty() || wsVisited.contains(cId)) continue;
        wsVisited.insert(cId);

        // request synchronously and feed into parser
        auto reply = get(workshopLinkFromId(cId));
        if (reply->error() != QNetworkReply::NoError) continue; // failed
        auto replyContents = reply->readAll();
        auto doc = htmlReadDoc(reinterpret_cast<xmlChar*>(replyContents.data()), nullptr, nullptr, HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
        auto root = std::make_shared<xmlpp::Element>(xmlDocGetRootElement(doc));

        if (needsInfo) { // update display name and title from html
            needsInfo = false;

            auto ele = root->find("//div[@class=\"collectionHeaderContent\"]//div[@class=\"workshopItemTitle\"]/text()");
            auto name = str(ele[0]);
            info["name"] = name;
            info["windowTitle"] = qs("Starbound - %1").arg(name);
        }

        // handle mod entries
        auto modNodes = root->find("//div[@class=\"collectionChildren\"]//div[@class=\"collectionItemDetails\"]");
        for (auto mn : modNodes) {
            auto id = workshopIdFromLink(str(mn->find(".//a/attribute::href")[0]));
            if (!wsMods.contains(id)) wsMods[id] = str(mn->find(".//div[@class=\"workshopItemTitle\"]/text()")[0]);
        }

        // and queue up subcollections
        auto collNodes = root->find("//div[@class=\"collectionChildren\"]//div[@class=\"workshopBrowseRow\"]/div[starts-with(@class, 'workshopItem')]");
        for (auto cn : collNodes) {
            auto id = workshopIdFromLink(str(cn->find(".//a/attribute::href")[0]));
            if (!id.isEmpty()) wsQueue.push_back(id);
        }

        if (needsExtCfg) { // find extcfg block
            needsExtCfg = false;

            auto ele = root->find("//div[@class=\"workshopItemDescriptionForCollection\"]//div[@class=\"bb_code\"][last()]/text()");
            if (!ele.empty()) {
                QString xcs; // demangle the text from workshop's hackiness
                for (auto e : ele) xcs.append(str(e)).append("\n");
                json["extCfg"] = parseJson(xcs.toUtf8()).object();
            }
        }

        xmlFreeDoc(doc);
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
    inst->save();
}
