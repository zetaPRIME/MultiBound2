#pragma once

#include <QString>
#include <QDir>

#define qs QStringLiteral

class QJsonDocument;

namespace MultiBound {
    namespace Util {

        inline QString splicePath(const QString& p1, const QString& p2) { return QDir::cleanPath(qs("%1/%2").arg(p1).arg(p2)); }

        QJsonDocument loadJson(const QString& path);
    }
}
