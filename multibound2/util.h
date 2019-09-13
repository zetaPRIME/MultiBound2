#pragma once

#include <QString>
#include <QDir>

#define qs QStringLiteral

class QJsonDocument;

namespace MultiBound {
    class Instance;
    namespace Util {

        inline QString splicePath(const QString& p1, const QString& p2) { return QDir::cleanPath(qs("%1/%2").arg(p1).arg(p2)); }
        inline QString splicePath(const QDir& d, const QString& p) { return splicePath(d.absolutePath(), p); }

        QJsonDocument loadJson(const QString& path);
        QJsonDocument parseJson(const QByteArray data);

        inline QString workshopLinkFromId(const QString& id) { return qs("https://steamcommunity.com/sharedfiles/filedetails/?id=%1").arg(id); }
        inline QString workshopIdFromLink(const QString& link) {
            constexpr auto f = QString::SectionSkipEmpty;
            return link.section("id=", 1, 1, f).section('&', 0, 0, f); // extract only id parameter
        }
        void updateFromWorkshop(Instance*, bool save = true);
    }
}
