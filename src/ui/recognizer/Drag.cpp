#include "RecognizerWidget.h"

#include "ui_RecognizerWidget.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>

/// Manual window chrome: the panel has no title bar or frame by
/// design, so the resize grip (corner drag with a diagonal cursor)
/// is implemented here. Installed in wireUi. Candidate-strip input
/// (drag-scroll, wheel, leave) lives in CandidateList itself.
bool RecognizerWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == ui->resizeGrip) {
        if (event->type() == QEvent::MouseButtonPress) {
            const auto *me = static_cast<const QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                resizing = true;
                resizePressGlobal = me->globalPosition().toPoint();
                resizePressSize = size();
                ui->resizeGrip->grabMouse();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            const auto *me = static_cast<const QMouseEvent *>(event);
            if (resizing && ((me->buttons() & Qt::LeftButton) != 0U)) {
                const QPoint delta = me->globalPosition().toPoint() - resizePressGlobal;
                applyPanelSize(resizePressSize.width() + delta.x(),
                               resizePressSize.height() + delta.y());
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            const auto *me = static_cast<const QMouseEvent *>(event);
            if (resizing && me->button() == Qt::LeftButton) {
                resizing = false;
                ui->resizeGrip->releaseMouse();
                return true;
            }
        }
        return false;
    }
    return QWidget::eventFilter(watched, event);
}
