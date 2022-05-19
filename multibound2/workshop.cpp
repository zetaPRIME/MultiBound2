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
#include <QHttpMultiPart>
#include <QEventLoop>

namespace { // utilities
    QHttpPart formPart(QString id, QString val) {
        QHttpPart pt;
        pt.setHeader(QNetworkRequest::ContentDispositionHeader, qs("form-data; name=\"%1\"").arg(id));
        pt.setBody(val.toUtf8());
        return pt;
    }

    QHttpPart endPart() {
        QHttpPart pt;
        pt.setHeader(QNetworkRequest::ContentDispositionHeader, qs("form-data"));
        return pt;
    }
}

void MultiBound::Util::updateFromWorkshop(MultiBound::Instance* inst, bool save) {
    updateStatus(qs("Fetching collection details..."));

    auto wsId = inst->workshopId();
    if (wsId.isEmpty()) return; // no workshop id attached

    auto& json = inst->json;

    auto info = json["info"].toObject();
    info["workshopId"] = wsId; // make sure id tag is present

    QHash<QString, QString> wsMods;
    std::deque<QString> modInfoQueue;
    std::deque<QString> wsQueue;
    QSet<QString> wsVisited;

    QNetworkAccessManager net;
    QEventLoop ev;
    auto getCollectionDetails = [&](QStringList ids) {
        QNetworkRequest req(QUrl("https://api.steampowered.com/ISteamRemoteStorage/GetCollectionDetails/v1/"));
        req.setRawHeader("Accept", "application/json");

        QHttpMultiPart form(QHttpMultiPart::FormDataType);
        form.append(formPart(qs("collectioncount"), QString::number(ids.count())));
        for (auto i = 0; i < ids.count(); ++i)
            form.append(formPart(qs("publishedfileids[%1]").arg(i), ids[i]));
        form.append(endPart());

        req.setHeader(QNetworkRequest::ContentTypeHeader, qs("multipart/form-data; boundary=%1").arg(QString::fromUtf8(form.boundary())));

        auto reply = net.post(req, &form);
        QObject::connect(reply, &QNetworkReply::finished, &ev, &QEventLoop::quit);
        ev.exec(); // wait for things
        reply->deleteLater(); // queue deletion on event loop, *after we're done*

        return QJsonDocument::fromJson(reply->readAll()).object()[qs("response")].toObject();
    };
    auto getItemDetails = [&](QStringList ids) {
        QNetworkRequest req(qs("https://api.steampowered.com/ISteamRemoteStorage/GetPublishedFileDetails/v1/"));
        req.setRawHeader("Accept", "application/json");

        QHttpMultiPart form(QHttpMultiPart::FormDataType);
        form.append(formPart(qs("itemcount"), QString::number(ids.count())));
        for (auto i = 0; i < ids.count(); ++i)
            form.append(formPart(qs("publishedfileids[%1]").arg(i), ids[i]));
        form.append(endPart());

        req.setHeader(QNetworkRequest::ContentTypeHeader, qs("multipart/form-data; boundary=%1").arg(QString::fromUtf8(form.boundary())));

        auto reply = net.post(req, &form);
        QObject::connect(reply, &QNetworkReply::finished, &ev, &QEventLoop::quit);
        ev.exec(); // wait for things
        reply->deleteLater(); // queue deletion on event loop, *after we're done*

        return QJsonDocument::fromJson(reply->readAll()).object()[qs("response")].toObject();
    };

    bool needsInfo = !info["lockInfo"].toBool();
    bool needsExtCfg = true;

    wsQueue.push_back(wsId); // it starts with one
    while (!wsQueue.empty()) {
        QString cId = wsQueue.front();
        wsQueue.pop_front();
        if (cId.isEmpty() || wsVisited.contains(cId)) continue;
        wsVisited.insert(cId);

        auto obj = getCollectionDetails(QStringList() << cId);

        if (needsInfo) { // update display name and title from html
            needsInfo = false;

            auto inf = getItemDetails(QStringList() << cId)["publishedfiledetails"].toArray()[0].toObject();
            auto name = inf["title"].toString();

            info["name"] = name;
            info["windowTitle"] = qs("Starbound - %1").arg(name);
        }

        auto children = obj["collectiondetails"].toArray()[0].toObject()["children"].toArray();
        for (auto ch : children) {
            auto c = ch.toObject();
            auto id = c["publishedfileid"].toString();
            auto ft = c["filetype"].toInt(-1);
            if (id.isEmpty()) continue;
            if (ft == 2) { // subcollection
                wsQueue.push_back(id);
            } else if (ft == 0) { // mod
                modInfoQueue.push_back(id);
            }
        }

        if (needsExtCfg) { // find extcfg block
            needsExtCfg = false;

            //json["extCfg"] = queryPage("eval('json_out = ' + $('div.workshopItemDescriptionForCollection div.bb_code').last().html().replace(/<br>/gi,'\\n') + '; json_out')").toJsonObject();
        }
    }

    if (modInfoQueue.size() > 0) {
        QStringList ids;
        for (auto& id : modInfoQueue) ids << id;
        //modInfoQueue.clear();

        auto r = getItemDetails(ids)["publishedfiledetails"].toArray();
        for (auto pfi : r) {
            auto inf = pfi.toObject();
            auto id = inf["publishedfileid"].toString();
            auto title = inf["title"].toString();
            if (id.isEmpty()) continue;

            wsMods[id] = title;
        }
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
