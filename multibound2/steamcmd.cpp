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
#if defined(Q_OS_WIN)
    QDir scd(Config::configPath);
    scd.cd("steamcmd");
    if (!scd.exists("steamcmd.exe")) { // download and install if not present
        updateStatus(qs("Installing steamcmd..."));

        QNetworkAccessManager net;
        QEventLoop ev;
        auto get = [&](QUrl url) {
            auto reply = net.get(QNetworkRequest(url));
            QObject::connect(reply, &QNetworkReply::finished, &ev, &QEventLoop::quit);
            ev.exec(); // wait for things
            reply->deleteLater(); // queue deletion on event loop, *after we're done*
            return reply;
        };

        auto scz = scd.absoluteFilePath("steamcmd.zip");
        QFile sczf(scz);
        sczf.open(QIODevice::ReadWrite | QIODevice::Truncate);
        sczf.write(get(qs("https://steamcdn-a.akamaihd.net/client/installer/steamcmd.zip"))->readAll());

        scp->start("powershell", QStringList() << "Expand-Archive" << scz << scd.absolutePath());
        if (!scp->waitForFinished()) return; // extraction failed for some reason or another, abort
    }
    scp->setProgram(scd.absoluteFilePath("steamcmd.exe"));
    scp->setWorkingDirectory(scd.absolutePath());
#elif defined(Q_OS_MACOS)
    if (QDir scd("~/Steam/steamcmd.sh"); scd.exists()) { // assume suggested install location
        scp->setProgram(scd.absolutePath());
    } else return;
#else
    // test with standard "which" command on linux
    scp->start("which", QStringList() << "steamcmd");
    if (!scp->waitForFinished()) return;
    scp->readAll(); // clear buffer

    scp->setProgram("steamcmd");
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
    bool wsUpd = true; // should update workshop-subscribed mods?

    QString wsScript, dlScript;
    QTextStream wss(&wsScript), dls(&dlScript);

    wss << qs("login anonymous\n");
    if (ws) { // if valid workshop, force proper root in case steamcmd defaults are incorrect
        QDir wsr(Config::workshopRoot);
        for (int i = 0; i < 4; i++) wsr.cdUp();
        wss << qs("force_install_dir ") << wsr.absolutePath() << qs("\n");
    }
    dls << qs("force_install_dir ") << Config::steamcmdDLRoot << qs("\n");

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
