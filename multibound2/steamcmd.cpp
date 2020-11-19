#include "util.h" // part of utility header

#include "data/config.h"
#include "data/instance.h"

#include <QJsonDocument>
#include <QJsonArray>

#include <QProcess>
#include <QFile>

#include <QSet>

void MultiBound::Util::updateMods(MultiBound::Instance* inst) {
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

    QString wsScript, dlScript;
    QTextStream wss(&wsScript), dls(&dlScript);

    wss << qs("login anonymous\n");
    dls << qs("force_install_dir ") << Config::steamcmdDLRoot << qs("\n");

    bool ws = true; // workshop active
    bool wsUpd = true; // should update workshop-subscribed mods?
    auto eh = qs("workshop_download_item 211820 ");
    for (auto id : workshop) {
        if (!workshopExclude.contains(id)) {
            if (ws && QDir(Config::workshopRoot).exists(id)) {
                if (wsUpd) wss << eh << id << qs("\n");
            } else dls << eh << id << qs("\n");
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

    scp->start();
    scp->waitForFinished();
}
