#include "util.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <QJSEngine>

#include <QDebug>

std::function<void(QString)> MultiBound::Util::updateStatus;

QJsonDocument MultiBound::Util::loadJson(const QString& path) {
    QFile f(path);
    f.open(QFile::ReadOnly);
    return parseJson(f.readAll());
}

QJsonDocument MultiBound::Util::parseJson(const QByteArray data) {
    QJsonParseError err;
    QJsonDocument json = QJsonDocument::fromJson(data, &err);

    // if it fails, try stripping comments via javascript
    if (err.error != QJsonParseError::NoError) {
        QJSEngine js;
        auto v = js.evaluate(qs("js=%1;").arg(QString(data)));
        auto func = js.evaluate(qs("JSON.stringify"));
        QJSValueList args;
        args.append(v);
        auto v2 = func.call(args);
        if (v2.isString()) json = QJsonDocument::fromJson(v2.toString().toUtf8(), &err);
    }

    if (err.error != QJsonParseError::NoError) qDebug() << err.errorString();
    return json;
}
