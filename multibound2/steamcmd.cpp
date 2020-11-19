#include "util.h" // part of utility header

#include "data/config.h"
#include "data/instance.h"

#include <QJsonDocument>
#include <QJsonArray>

#include <QProcess>
#include <QFile>
#include <QEventLoop>

#include <QSet>

namespace {
    const auto updMsg = qs("Updating mods...");
}

void MultiBound::Util::updateMods(MultiBound::Instance* inst) {
    updateStatus(updMsg);

    auto scp = new QProcess();
    scp->setProgram("steamcmd");

    auto& json = inst->json;

    QSet<QString> workshop;
    QSet<QString> workshopExclude;

    auto sources = json["assetSources"].toArray();
    for (auto s : sources) {
        if (s.isObject()) {
            auto src = s.toObject();
            auto type = src["type"].toString();
            if (type == "mod") {
                if (auto id = src["workshopId"]; id.isString()) workshop.insert(id.toString());
            } else if (type == "workshopAuto") {
                if (auto id = src["id"]; id.isString()) workshop.insert(id.toString());
            } else if (type == "workshopExclude") {
                if (auto id = src["id"]; id.isString()) workshopExclude.insert(id.toString());
            }
        }
    }

    int wsc = 0;

    QString wsScript, dlScript;
    QTextStream wss(&wsScript), dls(&dlScript);

    wss << qs("login anonymous\n");
    dls << qs("force_install_dir ") << Config::steamcmdDLRoot << qs("\n");

    bool ws = !Config::workshopRoot.isEmpty(); // workshop active; valid workshop root?
    bool wsUpd = true; // should update workshop-subscribed mods?
    auto eh = qs("workshop_download_item 211820 ");
    for (auto id : workshop) {
        if (!workshopExclude.contains(id)) {
            if (ws && QDir(Config::workshopRoot).exists(id)) {
                if (wsUpd) { wss << eh << id << qs("\n"); wsc++; }
            } else {dls << eh << id << qs("\n"); wsc++; }
        }
    }

    dls.flush();
    wss << dlScript;
    wss.flush();

    QString scriptPath = QDir(Config::steamcmdDLRoot).filePath("steamcmd.txt");
    QFile sf(scriptPath);
    sf.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text);
    (QTextStream(&sf)) << wsScript;
    sf.close();

    QStringList args;
    args << "+runscript" << scriptPath << "+quit";
    scp->setArguments(args);

    QEventLoop ev;
    QObject::connect(scp, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), &ev, &QEventLoop::quit);

    int wsp = 0;
    updateStatus(qs("%1 (%2/%3)").arg(updMsg).arg(wsp).arg(wsc));
    QObject::connect(scp, &QProcess::readyRead, [scp, wsc, &wsp] {
        while (scp->canReadLine()) {
            QString l = scp->readLine();
            if (l.startsWith(qs("Success. Downloaded item"))) {
                wsp++;
                updateStatus(qs("%1 (%2/%3)").arg(updMsg).arg(wsp).arg(wsc));
            }
        }
    });

    scp->start();
    ev.exec();
}
