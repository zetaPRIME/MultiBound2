#include "config.h"

#include "util.h"

#include <QDir>
#include <QStandardPaths>
#include <QDebug>

QString MultiBound::Config::configPath;
QString MultiBound::Config::instanceRoot;
QString MultiBound::Config::workshopRoot;
QString MultiBound::Config::starboundPath;
QString MultiBound::Config::starboundRoot;

void MultiBound::Config::load() {
    configPath = QStandardPaths::writableLocation(QStandardPaths::DataLocation); // already has "multibound" attached
    if (auto d = QDir(configPath); !d.exists()) d.mkpath(".");
    instanceRoot = Util::splicePath(configPath, "/instances/");
    if (auto d = QDir(instanceRoot); !d.exists()) d.mkpath(".");

    QDir home(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));

    starboundPath = Util::splicePath(home, "/.steam/steam/steamapps/common/Starbound/linux/run-client.sh");
    QDir r(starboundPath);
    while (r.dirName().toLower() != "starbound") {r.cdUp(); if (r.isRoot()) break;}
    starboundRoot = QDir::cleanPath(r.absolutePath());

    // if (starboundPath.contains("/steamapps/")) { }
    workshopRoot = Util::splicePath(home, "/.steam/steam/steamapps/workshop/content/211820/");

    //
}
