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

#include <QProcess>


// QDateTime::fromString(str, Qt::ISODate)

namespace { // clazy:excludeall=non-pod-global-static
    const auto releasesUrl = qs("https://api.github.com/repos/OpenStarbound/OpenStarbound/releases");
    //
    QJsonDocument releaseInfo;
    QJsonObject eligibleRelease;
    QString eligibleReleaseUrl;

    // os-specific things
#if defined(Q_OS_WIN)
    const auto pkgAssetName = qs("win_opensb.zip");
#elif defined (Q_OS_MACOS)
    const auto pkgAssetName = qs("osx_opensb.zip except not because we don't have code for this"); // it just won't find an eligible release
#else // linux
    const auto pkgAssetName = qs("linux_opensb.zip");
#endif
}

QJsonDocument& getReleaseInfo(bool forceRefresh = false) {
    if (!forceRefresh && !releaseInfo.isEmpty()) return releaseInfo;

    QNetworkAccessManager net;
    QEventLoop ev;

    QNetworkRequest req(releasesUrl);
    auto reply = net.get(req);
    QObject::connect(reply, &QNetworkReply::finished, &ev, &QEventLoop::quit);
    ev.exec(); // wait for things
    reply->deleteLater();

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

void MultiBound::Util::updateOSB() {
    findEligibleRelease();
    if (eligibleRelease.isEmpty()) return; // nothing

    // set up directory structure
    QDir osbd(Config::configPath);
    if (osbd.cd("opensb")) {
        // nuke old installation files if they exist
        osbd.removeRecursively();
        osbd.cdUp();
    }
    osbd.mkpath("opensb");
    osbd.cd("opensb");


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
    QObject::connect(reply, &QNetworkReply::finished, &ev, &QEventLoop::quit);
    ev.exec(); // wait for things
    reply->deleteLater();

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

    //ps.execute("chmod", QStringList() << "+x" << osbd.absoluteFilePath("starbound"));
    QFile ex(osbd.absoluteFilePath("starbound"));
    ex.setPermissions(ex.permissions() | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
#endif

    osbd.remove(fn); // clean up the zip after we're done

    // end
    updateStatus("");
}





















//
