#include "util.h" // part of utility header

// functions for OpenStarbound installation and update management

#include "data/config.h"

#include <cmath>

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QEventLoop>

#include <QMessageBox>
#include <QProcess>


// QDateTime::fromString(str, Qt::ISODate)

namespace { // clazy:excludeall=non-pod-global-static
    const auto releasesUrl = qs("https://api.github.com/repos/OpenStarbound/OpenStarbound/releases");
    //
    QJsonDocument releaseInfo;
    QJsonObject eligibleRelease;
    QString eligibleReleaseUrl;
    QJsonObject currentRelease;

    const auto ciUrl = qs("https://api.github.com/repos/OpenStarbound/OpenStarbound/actions/runs");

    // os-specific things
#if defined(Q_OS_WIN)
    const auto pkgAssetName = qs("win_opensb.zip");
    const auto ciPlatform = qs("Windows");
    const auto ciArtifact = qs("OpenStarbound-Windows-Client");
#elif defined (Q_OS_MACOS)
    const auto pkgAssetName = qs("osx_opensb.zip except not because we don't have code for this"); // it just won't find an eligible release
    const auto ciPlatform = qs("macOS");
    const auto ciArtifact = qs("and now we have a problem");
#else // linux
    const auto pkgAssetName = qs("linux_opensb.zip");
    const auto ciPlatform = qs("Ubuntu Linux");
    const auto ciArtifact = qs("OpenStarbound-Linux-Client");
#endif

    // some utility stuff
    QEventLoop ev;
    void await(QNetworkReply* reply) {
        QObject::connect(reply, &QNetworkReply::finished, &ev, &QEventLoop::quit);
        ev.exec(); // wait for things
        reply->deleteLater();
    }
}

QJsonDocument& getReleaseInfo(bool forceRefresh = false) {
    if (!forceRefresh && !releaseInfo.isEmpty()) return releaseInfo;

    QNetworkAccessManager net;

    QNetworkRequest req(releasesUrl);
    auto reply = net.get(req);
    await(reply);

    releaseInfo = QJsonDocument::fromJson(reply->readAll());
    return releaseInfo;
}

QJsonObject& findEligibleRelease(bool forceRefresh = false) {
    if (!forceRefresh && !eligibleRelease.isEmpty()) return eligibleRelease;
    getReleaseInfo(forceRefresh);

    auto ra = releaseInfo.array();

    // for now we assume releases are listed newest to oldest
    foreach (const auto& rv, ra) {
        auto r = rv.toObject();
        // loop through assets to find the one we want
        foreach (const auto& av, r[qs("assets")].toArray()) {
            auto a = av.toObject();
            if (a[qs("name")] == pkgAssetName) {
                eligibleRelease = r;
                eligibleReleaseUrl = a[qs("browser_download_url")].toString();
                return eligibleRelease;
            }
        }
    }

    return eligibleRelease;
}

void MultiBound::Util::openSBCheckForUpdates() {
    if (currentRelease.isEmpty()) {
        if (QFile f(Util::splicePath(Config::openSBRoot, "/release.json")); f.exists()) {
            f.open(QFile::ReadOnly);
            currentRelease = QJsonDocument::fromJson(f.readAll()).object();
        }
    }
    // if still empty, assume none and... prompt?
    if (currentRelease.isEmpty()) {
        // TODO
    }

    findEligibleRelease(true);
    if (eligibleRelease.isEmpty()) return; // nothing to show
    auto ed = QDateTime::fromString(eligibleRelease["published_at"].toString(), Qt::ISODate);
    auto cd = QDateTime::fromString(currentRelease["published_at"].toString(), Qt::ISODate);
    if (ed > cd) { // eligible is newer (or more extant) than current, prompt to update
        auto ver = eligibleRelease["name"].toString();
        if (ver.isEmpty()) ver = eligibleRelease["tag_name"].toString();
        if (ver.isEmpty()) ver = "unknown";

        auto res = QMessageBox::information(nullptr, "OpenStarbound update", QString("A new version of OpenStarbound (%1) is available. Would you like to install it?").arg(ver), QMessageBox::Yes, QMessageBox::No);
        if (res == QMessageBox::Yes) {
            openSBUpdate();
        }
        //
    }
}

void MultiBound::Util::openSBUpdate() {
    findEligibleRelease();
    if (eligibleRelease.isEmpty()) return; // nothing

    // set up directory structure
    QDir osbd(Config::openSBRoot);
    if (osbd.exists()) { // nuke old installation files if they exist
        osbd.removeRecursively();
    }
    osbd.mkpath("."); // make the directory exist (again)

    // download time!
    QNetworkAccessManager net;
    QEventLoop ev;
    const auto fn = qs("_download.zip");
    QFile f(osbd.absoluteFilePath(fn));
    f.open(QFile::WriteOnly);

    updateStatus("Downloading...");

    QNetworkRequest req(eligibleReleaseUrl);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy); // allow redirects
    auto reply = net.get(req);
    QObject::connect(reply, &QNetworkReply::downloadProgress, &ev, [](auto c, auto t) {
        updateStatus(QString("Downloading... %1\%").arg(std::floor((double)c/(double)t*100 + 0.5)));
    });
    QObject::connect(reply, &QNetworkReply::readyRead, &f, [&f, reply] {f.write(reply->readAll());});
    await(reply);

    f.flush();
    f.close();

    // now we unzip this fucker
    updateStatus("Extracting package...");
    QProcess ps;
    QObject::connect(&ps, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), &ev, &QEventLoop::quit);

#if defined(Q_OS_WIN)
    // powershell is weird but guaranteed to be there
    ps.start("powershell", QStringList() << "Expand-Archive" << "-DestinationPath" << osbd.absolutePath() << "-LiteralPath" << f.fileName());
    ev.exec();
#else
    // if you're running a desktop without unzip, huh???
    ps.start("unzip", QStringList() << f.fileName() << "-d" << osbd.absolutePath());
    ev.exec();

    foreach (auto fn, osbd.entryList(QDir::Files)) {
        if (!fn.contains(".")) { // no extension, assume it's a binary and fix permissions as such
            QFile ex(osbd.absoluteFilePath(fn));
            ex.setPermissions(ex.permissions() | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
        }
    }
#endif

    osbd.remove(fn); // clean up the zip after we're done
    updateStatus("");

    // finally, write out the release info
    QFile rf(osbd.absoluteFilePath("release.json"));
    rf.open(QFile::WriteOnly);
    rf.write(QJsonDocument(eligibleRelease).toJson(QJsonDocument::Indented));
    rf.flush();
    rf.close();

    currentRelease = eligibleRelease;
}

void MultiBound::Util::openSBUpdateCI() {
    QNetworkAccessManager net;

    QNetworkRequest req(releasesUrl);
    auto reply = net.get(req);
    await(reply);

    auto runs = QJsonDocument::fromJson(reply->readAll()).object()["workflow_runs"].toArray();

    QJsonObject ri;
    foreach (const auto& rv, runs) {
        auto r = rv.toObject();

        if (r["name"] == ciPlatform && r["head_branch"] == "main") {
            ri = r;
            break;
        }
    }

    if (ri.isEmpty()) {
        QMessageBox::critical(nullptr, "", "No matching artifacts found.");
        return;
    }














    //
}
