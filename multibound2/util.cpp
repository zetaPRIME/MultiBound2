#include "util.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <QJSEngine>

#include <QDebug>

QJsonDocument MultiBound::Util::loadJson(const QString& path) {
    QFile f(path);
    f.open(QFile::ReadOnly);
    auto fc = f.readAll();
    QJsonParseError err;
    QJsonDocument json = QJsonDocument::fromJson(fc, &err);

    // if it fails, try stripping comments via javascript
    if (err.error == QJsonParseError::IllegalNumber || err.error == QJsonParseError::IllegalValue) {
        QJSEngine js;
        auto v = js.evaluate(qs("js=%1;").arg(QString(fc)));
        auto func = js.evaluate(qs("JSON.stringify"));
        QJSValueList args;
        args.append(v);
        auto v2 = func.call(args);
        if (v2.isString()) json = QJsonDocument::fromJson(v2.toString().toUtf8());
    }

    return json;
}
