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

#include <QVersionNumber>

const QString MultiBound::Util::version = qs("v0.6.0");

namespace { // clazy:excludeall=non-pod-global-static
    const auto releasesUrl = qs("https://api.github.com/repos/zetaPRIME/MultiBound2/releases");
}

QVersionNumber getVer(QString a) {
    if (a.at(0) == 'v') a = a.mid(1);
    int suf = -1;
    auto v = QVersionNumber::fromString(a, &suf);

    if (suf > 0 && suf < a.length()) {
        auto c = a.at(suf).toLower();
        if (c.isLetter()) {
            auto vr = v.segments();
            vr.append(c.toLatin1() - ('a'-1));
            v = QVersionNumber(vr);
        }
    }

    return v;
}

// a > b
bool compareVersions(const QString& a, const QString& b = MultiBound::Util::version) {
    return getVer(a) > getVer(b);
}

// somewhat naive github version checking for Windows only
void MultiBound::Util::checkForUpdates() {
    updateStatus("Checking for updates...");

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

        if (compareVersions(ver)) { // prompt
            auto res = QMessageBox::information(nullptr, "Update Check", QString("A new version of MultiBound (%1) is available. Would you like to visit the release page?").arg(ver), QMessageBox::Yes, QMessageBox::No);
            if (res == QMessageBox::Yes) {
                QDesktopServices::openUrl(rel["html_url"].toString());
            }
        }
    }

    updateStatus("");
}
