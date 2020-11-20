#include "uitools.h"

#include <QEvent>
#include <QMenu>

using MultiBound::CheckableEventFilter;

bool CheckableEventFilter::eventFilter(QObject* obj, QEvent* e) {
    if (e->type() == QEvent::MouseButtonRelease) {
        if (auto m = qobject_cast<QMenu*>(obj); m) {
            if (auto a = m->activeAction(); a && a->isCheckable()) {
                a->trigger();
                return true;
            }
        }
    }

    return QObject::eventFilter(obj, e);
}
