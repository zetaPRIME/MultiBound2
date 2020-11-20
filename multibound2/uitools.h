#pragma once

#include <QObject>

namespace MultiBound {
    class CheckableEventFilter : public QObject {
        Q_OBJECT
    public:
        bool eventFilter(QObject* obj, QEvent* e) override;
    };
}
