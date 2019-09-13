#include "config.h"

#include "util.h"

#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>

#include <QJsonDocument>
#include <QJsonObject>

#include <QDebug>

QString MultiBound::Config::configPath;
QString MultiBound::Config::instanceRoot;
QString MultiBound::Config::workshopRoot;
QString MultiBound::Config::starboundPath;
QString MultiBound::Config::starboundRoot;

void MultiBound::Config::load() {
    // defaults
#if defined(Q_OS_WIN)
    configPath = QCoreApplication::applicationDirPath(); // same place as the exe on windows, like mb1
    starboundPath = QDir::cleanPath(qs("C:/Program Files (x86)/Steam/SteamApps/common/Starbound/win64/starbound.exe"));
#elif defined(Q_OS_MACOS)
    configPath = QStandardPaths::writableLocation(QStandardPaths::DataLocation); // already has "multibound" attached

    QDir home(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    starboundPath = Util::splicePath(home, "/Library/Application Support/Steam/steamapps/common/Starbound/osx/Starbound.app/Contents/MacOS/starbound");
#else
    configPath = QStandardPaths::writableLocation(QStandardPaths::DataLocation); // already has "multibound" attached

    QDir home(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    starboundPath = Util::splicePath(home, "/.steam/steam/steamapps/common/Starbound/linux/run-client.sh");
#endif
    if (auto d = QDir(configPath); !d.exists()) d.mkpath(".");
    instanceRoot = Util::splicePath(configPath, "/instances/");

    QJsonObject cfg;
    if (QFile f(Util::splicePath(configPath, "/config.json")); f.exists()) {
        f.open(QFile::ReadOnly);
        cfg = QJsonDocument::fromJson(f.readAll()).object();

        starboundPath = cfg["starboundPath"].toString(starboundPath);
        instanceRoot = cfg["instanceRoot"].toString(instanceRoot);
    }

    if (auto d = QDir(instanceRoot); !d.exists()) d.mkpath(".");

    QDir r(starboundPath);
    while (r.dirName().toLower() != "starbound") { auto old = r; r.cdUp(); if (r == old) break; }
    starboundRoot = QDir::cleanPath(r.absolutePath());

    while (r.dirName().toLower() != "steamapps") { auto old = r; r.cdUp(); if (r == old) break; }
    workshopRoot = Util::splicePath(r.absolutePath(), "/workshop/content/211820/");

}

void MultiBound::Config::save() {
    QJsonObject cfg;
    cfg["starboundPath"] = starboundPath;
    cfg["instanceRoot"] = instanceRoot;

    QFile f(Util::splicePath(configPath, "/config.json"));
    f.open(QFile::WriteOnly);
    f.write(QJsonDocument(cfg).toJson());
}
