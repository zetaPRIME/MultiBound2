#pragma once

#include <memory>

#include <QJsonObject>

namespace MultiBound {
    class Instance {

        //
    public:
        QString path;
        QJsonObject json;

        Instance() = default;
        ~Instance() = default;

        QString displayName();

        QString evaluatePath(const QString&);
        bool load();
        bool save();
        bool launch();

        // statics
        static std::shared_ptr<Instance> loadFrom(const QString& path);

    };
}
