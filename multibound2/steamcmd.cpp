#include "util.h" // part of utility header

#include "data/config.h"
#include "data/instance.h"

#include <QJsonDocument>
#include <QJsonArray>

#include <QProcess>
#include <QFile>
#include <QEventLoop>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDataStream>

#include <QSet>

namespace {
    const auto updMsg = qs("Updating mods...");
}

void MultiBound::Util::updateMods(MultiBound::Instance* inst) {
    auto scp = new QProcess();
    QEventLoop ev;
    QObject::connect(scp, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), &ev, &QEventLoop::quit);
#if defined(Q_OS_WIN)
    QDir scd(Config::configPath);
    scd.mkpath("steamcmd"); // make sure it exists
    scd.cd("steamcmd");

    if (!scd.exists("steamcmd.exe")) { // download and install if not present
        updateStatus(qs("Downloading steamcmd..."));
        scp->start("powershell", QStringList() << "curl -o"<< scd.absoluteFilePath("steamcmd.zip") << "https://steamcdn-a.akamaihd.net/client/installer/steamcmd.zip");
        auto scz = scd.absoluteFilePath("steamcmd.zip");

        ev.exec();
        scp->start("powershell", QStringList() << "Expand-Archive"<< "-DestinationPath" << scd.absolutePath() << "-LiteralPath" << scz);
        ev.exec();
        if (scp->exitCode() != 0) return; // extraction failed for some reason or another, abort

        // finish installing
        updateStatus(qs("Installing steamcmd..."));
        scp->setWorkingDirectory(scd.absolutePath());
        scp->start(scd.absoluteFilePath("steamcmd.exe"), QStringList() << "+exit");
        ev.exec();

        scp->readAll(); // and clean up buffer
    }
    scp->setProgram(scd.absoluteFilePath("steamcmd.exe"));
    scp->setWorkingDirectory(scd.absolutePath());
#else
    // test with standard "which" command on linux and mac
    if (scp->execute("which", QStringList() << "steamcmd") == 0) { // use system steamcmd if present
        scp->readAll(); // clear buffer
        scp->setProgram("steamcmd");
    } else { // use our own copy, and install automatically if necessary
        scp->readAll(); // clear buffer

        QDir scd(Config::configPath);
        scd.mkpath("steamcmd"); // make sure it exists
        scd.cd("steamcmd");

        if (!scd.exists("steamcmd.sh")) { // install local copy
            updateStatus(qs("Downloading steamcmd..."));
#if defined(Q_OS_MACOS)
            auto scUrl = qs("https://steamcdn-a.akamaihd.net/client/installer/steamcmd_osx.tar.gz");
#else
            auto scUrl = qs("https://steamcdn-a.akamaihd.net/client/installer/steamcmd_linux.tar.gz");
#endif
            scp->setWorkingDirectory(scd.absolutePath());
            scp->start("bash", QStringList() << "-c" << qs("curl -sqL \"%1\" | tar -xzvf -").arg(scUrl));
            ev.exec();
            if (scp->exitCode() != 0) return; // something went wrong, abort

            // finish installing
            updateStatus(qs("Installing steamcmd..."));
            scp->setWorkingDirectory(scd.absolutePath());
            scp->start(scd.absoluteFilePath("steamcmd.sh"), QStringList() << "+exit");
            ev.exec();

            scp->readAll(); // and clean up buffer
        }
        scp->setProgram(scd.absoluteFilePath("steamcmd.sh"));
        scp->setWorkingDirectory(scd.absolutePath());
    }
#endif

    updateStatus(updMsg);

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

    bool ws = !Config::workshopRoot.isEmpty(); // workshop active; valid workshop root?
    bool wsUpd = Config::steamcmdUpdateSteamMods; // should update workshop-subscribed mods?

    QString wsScript, dlScript;
    QTextStream wss(&wsScript), dls(&dlScript);

    if (ws) { // if valid workshop, force proper root in case steamcmd defaults are incorrect
        QDir wsr(Config::workshopRoot);
        for (int i = 0; i < 4; i++) wsr.cdUp();
        wss << qs("force_install_dir ") << wsr.absolutePath() << qs("\n");
    }
    wss << qs("login anonymous\n"); // log in after forcing install dir
    
    dls << qs("logout\n"); // we need to set the install dir while logged out now (sigh)
    dls << qs("force_install_dir ") << Config::steamcmdDLRoot << qs("\n");
    dls << qs("login anonymous\n");

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
