#include "widgets/tooltip/Tooltip.h"

#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int hideDelayMs = 400;
} // namespace

Tooltip::Tooltip(QWidget *parent) : QWidget(parent, Qt::ToolTip) {
    // Tooltip type (never a top-level): no decorations on any
    // backend, no mouse grab (sources keep hover-tracking), no
    // activation. No NoDropShadowWindowHint: elevation is the
    // server's drop shadow (compositor-drawn around the window
    // shape), never a client effect. Click-outside/Escape dismiss
    // via the application-wide filter below instead of native
    // popup behavior.
    setAttribute(Qt::WA_ShowWithoutActivating);
    // Translucency stays: the card is rounded, so the corners show
    // the parent through (no shadow is drawn here — margins are
    // zero and the window is exactly the card).
    setAttribute(Qt::WA_TranslucentBackground);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    tipCard = new QWidget(this);
    tipCard->setObjectName(QStringLiteral("tooltipCard"));
    tipCard->setStyleSheet(QStringLiteral("QWidget#tooltipCard { "
                                          "background-color: palette(tooltip-base); "
                                          "border: 1px solid palette(mid); "
                                          "border-radius: 10px; }"));
    outer->addWidget(tipCard);

    showTimer = new QTimer(this);
    showTimer->setSingleShot(true);
    connect(showTimer, &QTimer::timeout, this, &Tooltip::onShowTimeout);
    hideTimer = new QTimer(this);
    hideTimer->setSingleShot(true);
    connect(hideTimer, &QTimer::timeout, this, &Tooltip::onHideTimeout);
    qApp->installEventFilter(this);
}

void Tooltip::hover() {
    hideTimer->stop();
    if (isVisible()) {
        refresh();
        placeAndShow();
    } else {
        showTimer->start(showDelayMs);
    }
}

void Tooltip::unhover() {
    showTimer->stop();
    hideTimer->start(hideDelayMs);
}

void Tooltip::onShowTimeout() {
    refresh();
    placeAndShow();
}

void Tooltip::onHideTimeout() {
    // Cursor made it onto the card: leave it open. The window is
    // exactly the card now (zero margins), so either rect tests
    // the same area.
    const QPoint inCard = tipCard->mapFromGlobal(QCursor::pos());
    if (!tipCard->rect().contains(inCard)) {
        hide();
    }
}

void Tooltip::placeAndShow() {
    // The window minimum tracks stale layout minimums (grid span
    // minimums lag content changes), blocking shrink; clear it so
    // the card always fits current content, then fit.
    setMinimumSize(QSize(0, 0));
    adjustSize();
    // The window is the card: park its top-left at the cursor
    // offset, clamped to the screen.
    const QPoint at = QCursor::pos();
    QPoint pos = at + QPoint(16, 16);
    if (QScreen *screen = QGuiApplication::screenAt(at)) {
        const QRect room = screen->availableGeometry();
        pos.setX(qMin(pos.x(), room.right() - width()));
        pos.setY(qMin(pos.y(), room.bottom() - height()));
    }
    move(pos);
    show();
}

bool Tooltip::eventFilter(QObject *watched, QEvent *event) {
    // Dismiss on outside press / Escape (tooltip windows have no
    // native click-away). Never consume: interaction flows through.
    if (isVisible()) {
        if (event->type() == QEvent::MouseButtonPress) {
            const auto *me = static_cast<const QMouseEvent *>(event);
            if (!geometry().contains(mapFromGlobal(me->globalPosition().toPoint()))) {
                hide();
            }
        } else if (event->type() == QEvent::KeyPress &&
                   static_cast<const QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            hide();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void Tooltip::enterEvent(QEnterEvent *event) {
    // Cursor reached the card: cancel a pending source-leave dismiss.
    hideTimer->stop();
    QWidget::enterEvent(event);
}

void Tooltip::leaveEvent(QEvent *event) {
    // Same grace as leaving the source: the cursor may be heading
    // back (which re-hovers instantly).
    unhover();
    QWidget::leaveEvent(event);
}

void Tooltip::changeEvent(QEvent *event) {
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ThemeChange) {
        // Re-setting the identical sheet is the re-resolve: without
        // it the card keeps the old theme's tooltip-base/mid while
        // the labels already follow the new one.
        tipCard->setStyleSheet(tipCard->styleSheet());
        if (isVisible()) {
            refresh();
        }
    }
    QWidget::changeEvent(event);
}
