#include "instance.h"
using MultiBound::Instance;

#include "data/config.h"
#include "util.h"

#include <QDebug>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <QDir>
#include <QFile>

#include <QSet>

#include <QProcess>

QString Instance::displayName() {
    return json["info"].toObject()["name"].toString(path);
}

QString Instance::workshopId() {
    auto info = json["info"].toObject();
    auto id = info["workshopId"].toString();
    if (id.isEmpty()) id = Util::workshopIdFromLink(info["workshopLink"].toString());
    return id;
}

QString Instance::evaluatePath(const QString& p) {
    auto f = QString::SectionSkipEmpty;
    auto pfx = p.section(':', 0, 0, f);
    auto body = p.section(':', 1, -1, f);

    if (pfx == "sb") return Util::splicePath(Config::starboundRoot, body);
    else if (pfx == "ws" || pfx == "workshop") {
        if (auto s = Util::splicePath(Config::workshopRoot, body); QDir(s).exists()) return s;
        return Util::splicePath(Config::steamcmdWorkshopRoot, body);
    } else if (pfx == "inst") return Util::splicePath(path, body);

    return QDir::cleanPath(p);
}

bool Instance::load() {
    QDir d(path);
    if (!d.exists("instance.json")) return false;

    json = Util::loadJson(d.absoluteFilePath("instance.json")).object();

    return true;
}

bool Instance::save() {
    QDir(path).mkpath(".");
    QFile f(QDir(path).absoluteFilePath("instance.json"));
    f.open(QFile::WriteOnly);
    f.write(QJsonDocument(json).toJson());
    return true;
}

bool Instance::launch() {
    // determine path ahead of time
    QString sbinitPath = Util::splicePath(Config::configPath, "_init.config");

    // and some logic for switching to OpenSB
    bool useOSB = Config::useOpenSB;
    auto osbRoot = Config::openSBUseDev ? Config::openSBDevRoot : Config::openSBRoot;
    if (!QDir(osbRoot).exists("starbound")) useOSB = false;

    {
        // construct sbinit
        QJsonObject sbinit;

        auto extCfg = json["extCfg"].toObject(); // extra config from steam workshop collections

        // assemble default configuration
        QJsonObject defaultCfg;
        defaultCfg["gameServerBind"] = "*";
        defaultCfg["queryServerBind"] = "*";
        defaultCfg["rconServerBind"] = "*";
        defaultCfg["allowAssetsMismatch"] = true;
        defaultCfg["maxTeamSize"] = 64; // more than you'll ever need, because why is there even a limit!?

        if (auto server = extCfg["defaultServer"].toString(); !server.isEmpty()) {
            QJsonObject ts;
            constexpr auto f = QString::SectionSkipEmpty;
            ts["multiPlayerAddress"] = server.section(':', 0, 0, f);
            ts["multiPlayerPort"] = server.section(':', 1, 1, f);
            defaultCfg["title"] = ts;
        }

        sbinit["defaultConfiguration"] = defaultCfg;

        // storage directory
        if (auto s = json["savePath"]; s.isString()) sbinit["storageDirectory"] = evaluatePath(s.toString());
        else sbinit["storageDirectory"] = evaluatePath("inst:/storage/");

        // and put together asset list
        QJsonArray assets;
        assets.append(evaluatePath("sb:/assets/"));
        if (useOSB) {
            assets.append(Util::splicePath(osbRoot, "/data/"));
        }

        QSet<QString> workshop;
        QSet<QString> workshopExclude;

        auto sources = json["assetSources"].toArray();
        for (auto s : sources) {
            if (s.isString()) assets.append(evaluatePath(s.toString()));
            else if (s.isObject()) {
                auto src = s.toObject();
                auto type = src["type"].toString();
                if (type == "mod") {
                    if (auto p = src["path"]; p.isString()) assets.append(evaluatePath(p.toString()));
                    else if (auto id = src["workshopId"]; id.isString()) workshop.insert(id.toString());
                } else if (type == "workshopAuto") {
                    if (auto id = src["id"]; id.isString()) workshop.insert(id.toString());
                } else if (type == "workshopExclude") {
                    if (auto id = src["id"]; id.isString()) workshopExclude.insert(id.toString());
                }
            }
        }

        for (auto id : workshop) if (!workshopExclude.contains(id)) assets.append(evaluatePath(qs("workshop:/") + id));

        sbinit["assetDirectories"] = assets;

        // write to disk
        QFile f(sbinitPath);
        f.open(QFile::WriteOnly);
        f.write(QJsonDocument(sbinit).toJson());
    }

    // then launch...
    QStringList param;
    param << "-bootconfig" << QDir::toNativeSeparators(sbinitPath);
    QProcess sb;
    QDir wp(Config::starboundPath);
    wp.cdUp();
    sb.setWorkingDirectory(wp.absolutePath());
    auto exec = Config::starboundPath;
    if (useOSB) exec = Util::splicePath(osbRoot, "/starbound");
    sb.start(exec, param);
    sb.waitForFinished(-1);
    qDebug() << sb.errorString();

    return true;
}

std::shared_ptr<Instance> Instance::loadFrom(const QString& path) {
    QString ipath;
    if (QDir(path).isAbsolute()) ipath = path;
    else ipath = Util::splicePath(Config::instanceRoot, path);
    if (!QDir(ipath).exists("instance.json")) return nullptr;

    auto inst = std::make_shared<Instance>();
    inst->path = ipath;
    if (!inst->load()) return nullptr;
    return inst;
}
