#include "util.h" // part of utility header

// version tracking and update checking

#include <QCoreApplication>
#include <QDir>
#include <QDateTime>

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QEventLoop>

#include <QMessageBox>
#include <QDesktopServices>

const QString MultiBound::Util::version = qs("v0.5.5");

namespace { // clazy:excludeall=non-pod-global-static
    const auto releasesUrl = qs("https://api.github.com/repos/zetaPRIME/MultiBound2/releases");
}

// somewhat naive github version checking for Windows only
void MultiBound::Util::checkForUpdates() {
#if defined(Q_OS_WIN)
    updateStatus("Checking for updates...");

    // grab install time for later
    auto binDate = QFileInfo(QCoreApplication::applicationFilePath()).birthTime();

    // fetch release info
    QNetworkAccessManager net;
    QEventLoop ev;

    QNetworkRequest req(releasesUrl);
    auto reply = net.get(req);
    QObject::connect(reply, &QNetworkReply::finished, &ev, &QEventLoop::quit);
    ev.exec(); // wait for things
    reply->deleteLater();

    auto releaseInfo = QJsonDocument::fromJson(reply->readAll());

    auto ra = releaseInfo.array();
    QJsonObject rel;

    // for now we assume releases are listed newest to oldest
    foreach (const auto& rv, ra) {
        auto r = rv.toObject();
        // loop through assets to find the one we want
        foreach (const auto& av, r[qs("assets")].toArray()) {
            auto a = av.toObject();
            if (a[qs("name")].toString().contains("-windows.zip")) {
                rel = r;
                break;
            }
        }
        if (!rel.isEmpty()) break;
    }

    if (!rel.isEmpty()) { // we found a release, check if it's eligible
        auto ver = rel["tag_name"].toString();
        auto date = QDateTime::fromString(rel["published_at"].toString(), Qt::ISODate);

        if (date > binDate && ver != Util::version) { // prompt
            auto res = QMessageBox::information(nullptr, "Update Check", QString("A new version of MultiBound (%1) is available. Would you like to download it?").arg(ver), QMessageBox::Yes, QMessageBox::No);
            if (res == QMessageBox::Yes) {
                QDesktopServices::openUrl(rel["html_url"].toString());
            }
        }
    }

    updateStatus("");
#endif
}
