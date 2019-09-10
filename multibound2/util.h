#pragma once

#include <QString>
#include <QDir>

#define qs QStringLiteral

class QJsonDocument;

namespace MultiBound {
    namespace Util {

        inline QString splicePath(const QString& p1, const QString& p2) { return QDir::cleanPath(qs("%1/%2").arg(p1).arg(p2)); }
        inline QString splicePath(const QDir& d, const QString& p) { return splicePath(d.absolutePath(), p); }

        QJsonDocument loadJson(const QString& path);
    }
}
