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
QString MultiBound::Config::steamcmdDLRoot;
QString MultiBound::Config::steamcmdWorkshopRoot;
QString MultiBound::Config::starboundPath;
QString MultiBound::Config::starboundRoot;
QString MultiBound::Config::steamUsername;
QString MultiBound::Config::steamPassword;

bool MultiBound::Config::steamcmdEnabled = true;
bool MultiBound::Config::steamcmdUpdateSteamMods = true;

void MultiBound::Config::load() {
    // defaults
    configPath = QStandardPaths::writableLocation(QStandardPaths::DataLocation); // already has "multibound" attached
#if defined(Q_OS_WIN)
    //; // same place as the exe on windows, like mb1
    if (QDir(QCoreApplication::applicationDirPath()).exists("config.json"))
        configPath = QCoreApplication::applicationDirPath();
    starboundPath = QDir::cleanPath(qs("C:/Program Files (x86)/Steam/SteamApps/common/Starbound/win64/starbound.exe"));
#elif defined(Q_OS_MACOS)
    QDir home(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    starboundPath = Util::splicePath(home, "/Library/Application Support/Steam/steamapps/common/Starbound/osx/Starbound.app/Contents/MacOS/starbound");
#else
    QDir home(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    starboundPath = Util::splicePath(home, "/.steam/steam/steamapps/common/Starbound/linux/run-client.sh");
#endif
    if (auto d = QDir(configPath); !d.exists()) d.mkpath(".");
    instanceRoot = Util::splicePath(configPath, "/instances/");

    steamcmdDLRoot = configPath;

    QJsonObject cfg;
    if (QFile f(Util::splicePath(configPath, "/config.json")); f.exists()) {
        f.open(QFile::ReadOnly);
        cfg = QJsonDocument::fromJson(f.readAll()).object();

        starboundPath = cfg["starboundPath"].toString(starboundPath);
        instanceRoot = cfg["instanceRoot"].toString(instanceRoot);
        steamcmdDLRoot = cfg["steamcmdRoot"].toString(steamcmdDLRoot);

        steamcmdUpdateSteamMods = cfg["steamcmdUpdateSteamMods"].toBool(steamcmdUpdateSteamMods);
        steamUsername = cfg["steamUsername"].toString(steamUsername);
        steamPassword = cfg["steamPassword"].toString(steamPassword);
    }

    if (auto d = QDir(instanceRoot); !d.exists()) d.mkpath(".");

    QDir r(starboundPath);
    while (r.dirName().toLower() != "starbound" || !r.exists()) { auto old = r; r.cdUp(); if (r == old) break; }
    starboundRoot = QDir::cleanPath(r.absolutePath());

    while (r.dirName().toLower() != "steamapps") { auto old = r; r.cdUp(); if (r == old) break; }
    if (auto rr = r.absolutePath(); rr.length() >= 10) workshopRoot = Util::splicePath(rr, "/workshop/content/211820/");

    steamcmdWorkshopRoot = Util::splicePath(QDir(steamcmdDLRoot).absolutePath(), "/steamapps/workshop/content/211820/");

}

void MultiBound::Config::save() {
    QJsonObject cfg;
    cfg["starboundPath"] = starboundPath;
    cfg["instanceRoot"] = instanceRoot;
    cfg["steamcmdRoot"] = steamcmdDLRoot;
    cfg["steamUsername"] = steamUsername;
    cfg["steamPassword"] = steamPassword;

    cfg["steamcmdUpdateSteamMods"] = steamcmdUpdateSteamMods;

    QFile f(Util::splicePath(configPath, "/config.json"));
    f.open(QFile::WriteOnly);
    f.write(QJsonDocument(cfg).toJson());
}
