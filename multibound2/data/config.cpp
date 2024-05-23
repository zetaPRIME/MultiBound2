#include "config.h"

#include "util.h"

#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QSettings>

#include <QJsonDocument>
#include <QJsonObject>

#include <QCryptographicHash>

#include <QDebug>

QString MultiBound::Config::configPath;
QString MultiBound::Config::instanceRoot;
QString MultiBound::Config::workshopRoot;
QString MultiBound::Config::steamcmdDLRoot;
QString MultiBound::Config::steamcmdWorkshopRoot;
QString MultiBound::Config::starboundPath;
QString MultiBound::Config::starboundRoot;

bool MultiBound::Config::steamcmdEnabled = true;
bool MultiBound::Config::steamcmdUpdateSteamMods = true;

bool MultiBound::Config::openSBEnabled = true;
bool MultiBound::Config::openSBUseCIBuild = false;
bool MultiBound::Config::openSBUseDevBranch = false;
bool MultiBound::Config::openSBOffered = false;
QString MultiBound::Config::openSBRoot;
QString MultiBound::Config::openSBCIRoot;
QString MultiBound::Config::openSBDevRoot;

namespace vdf = tyti::vdf;

namespace {
    [[maybe_unused]] inline bool isX64() { return QSysInfo::currentCpuArchitecture().contains(QLatin1String("64")); }
}

void MultiBound::Config::load() {
    // defaults
    configPath = QStandardPaths::writableLocation(QStandardPaths::DataLocation); // already has "multibound" attached
#if defined(Q_OS_WIN)
    // same place as the exe on windows, like mb1
    if (QDir(QCoreApplication::applicationDirPath()).exists("config.json"))
        configPath = QCoreApplication::applicationDirPath();

    // find Steam path
#if defined(Q_PROCESSOR_X86_64)
    QSettings sreg(R"(HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Valve\Steam)", QSettings::NativeFormat);
    QString steamPath = QDir::cleanPath(qs("C:/Program Files (x86)/Steam"));
#else
    QSettings sreg(R"(HKEY_LOCAL_MACHINE\SOFTWARE\Valve\Steam)", QSettings::NativeFormat);
    QString steamPath = QDir::cleanPath(qs("C:/Program Files/Steam"));
#endif
    if (auto ip = sreg.value("InstallPath"); ip.type() == QVariant::String && !ip.isNull()) steamPath = QDir::cleanPath(ip.toString());

    if (isX64()) starboundPath = Util::splicePath(steamPath, "/SteamApps/common/Starbound/win64/starbound.exe");
    else starboundPath = Util::splicePath(steamPath, "/SteamApps/common/Starbound/win32/starbound.exe");
#elif defined(Q_OS_MACOS)
    QDir home(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    starboundPath = Util::splicePath(home, "/Library/Application Support/Steam/steamapps/common/Starbound/osx/Starbound.app/Contents/MacOS/starbound");
#else
    QDir home(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    starboundPath = Util::splicePath(home, "/.steam/steam/steamapps/common/Starbound/linux/run-client.sh");
#endif
    if (auto d = QDir(configPath); !d.exists()) d.mkpath(".");
    instanceRoot = Util::splicePath(configPath, "/instances/");

    openSBRoot = Util::splicePath(configPath, "/opensb/");
    openSBCIRoot = Util::splicePath(configPath, "/opensb-ci/");
    openSBDevRoot = Util::splicePath(configPath, "/opensb-dev/");

    steamcmdDLRoot = configPath;

    QJsonObject cfg;
    if (QFile f(Util::splicePath(configPath, "/config.json")); f.exists()) {
        f.open(QFile::ReadOnly);
        cfg = QJsonDocument::fromJson(f.readAll()).object();

        starboundPath = cfg["starboundPath"].toString(starboundPath);
        instanceRoot = cfg["instanceRoot"].toString(instanceRoot);
        steamcmdDLRoot = cfg["steamcmdRoot"].toString(steamcmdDLRoot);

        steamcmdUpdateSteamMods = cfg["steamcmdUpdateSteamMods"].toBool(steamcmdUpdateSteamMods);

        openSBEnabled = cfg["openSBEnabled"].toBool(openSBEnabled);
        openSBUseCIBuild = cfg["openSBUseCIBuild"].toBool(openSBUseCIBuild);
        openSBUseDevBranch = cfg["openSBUseDevBranch"].toBool(openSBUseDevBranch);
        openSBOffered = cfg["openSBOffered"].toBool(openSBOffered);
    }

    if (auto d = QDir(instanceRoot); !d.exists()) d.mkpath(".");

    QDir r(starboundPath);
    while (r.dirName().toLower() != "starbound" || !r.exists()) { auto old = r; r.cdUp(); if (r == old) break; }
    starboundRoot = QDir::cleanPath(r.absolutePath());

    while (r.dirName().toLower() != "steamapps") { auto old = r; r.cdUp(); if (r == old) break; }
    if (auto rr = r.absolutePath(); rr.length() >= 10) workshopRoot = Util::splicePath(rr, "/workshop/content/211820/");

    steamcmdWorkshopRoot = Util::splicePath(QDir(steamcmdDLRoot).absolutePath(), "/steamapps/workshop/content/211820/");

    verify();
}

void MultiBound::Config::verify() {
    // stub
}

void MultiBound::Config::save() {
    verify();

    QJsonObject cfg;
    cfg["starboundPath"] = starboundPath;
    cfg["instanceRoot"] = instanceRoot;
    cfg["steamcmdRoot"] = steamcmdDLRoot;

    cfg["steamcmdUpdateSteamMods"] = steamcmdUpdateSteamMods;

    cfg["openSBEnabled"] = openSBEnabled;
    cfg["openSBUseCIBuild"] = openSBUseCIBuild;
    cfg["openSBUseDevBranch"] = openSBUseDevBranch;
    cfg["openSBOffered"] = openSBOffered;

    QFile f(Util::splicePath(configPath, "/config.json"));
    f.open(QFile::WriteOnly);
    f.write(QJsonDocument(cfg).toJson());
}
