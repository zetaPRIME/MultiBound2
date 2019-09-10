#include "workshop.h"

#include <deque>

#include <QSet>

void MultiBound::Util::updateFromWorkshop(MultiBound::Instance* inst) {
    auto wsId = inst->workshopId();
    if (wsId.isEmpty()) return; // no workshop id attached

    auto& json = inst->json;

    auto info = json["info"].toObject();

    QSet<QString> modIds;
    std::deque<QString> wsQueue;
    QSet<QString> wsVisited;

    bool needsInfo = true;

    wsQueue.push_back(wsId); // it starts with one
    while (!wsQueue.empty()) {
        QString cId = wsQueue.front();
        wsQueue.pop_front();
        wsVisited.insert(cId);

        // request synchronously and feed into parser

        if (needsInfo) { // update display name from html
            needsInfo = false;
            //
        }
    }

    json["info"] = info; // update info in json body

    // rebuild assetSources with new contents
    //

    // commit
    inst->save();
}
