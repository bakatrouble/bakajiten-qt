#include "CanvasWidget.h"

#include "core/palette/MaterialPalette.h"
#include "core/settings/Settings.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {
/// pen-space edge: the recognizer's native coordinate space.
constexpr double kBox = 255.0;
} // namespace

CanvasWidget::CanvasWidget(QWidget *parent) : QWidget(parent) {
    updateMinimum();
    // Plain expanding both ways: squareness comes from the panel
    // (applyPanelSize keeps w-h on the square-canvas line), not from
    // height-for-width — an expanding height stretches past it to
    // fill the row, and a fixed one collapses to the minimum.
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(false);
}

void CanvasWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::FontChange) {
        updateMinimum();
    }
    QWidget::changeEvent(event);
}

void CanvasWidget::updateMinimum() {
    // ~220px at the 9pt design font, following the widget's own
    // font thereafter (interface scale included).
    const int side = qMax(160, qRound(QFontMetricsF(font()).height() * 15.0));
    setMinimumSize(side, side);
}

void CanvasWidget::setStrokeData(const QVector<QVector<QPointF>> *finished,
                                 const QVector<QPointF> *open) {
    this->finished = finished;
    this->open = open;
    update();
}

void CanvasWidget::setGridVisible(bool visible) {
    gridVisible = visible;
    update();
}

QPointF CanvasWidget::to255(const QPointF &atPx) const {
    const QRectF r = rect();
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPointF(0.0, 0.0);
    }
    const double x = qBound(0.0, (atPx.x() - r.left()) / r.width() * kBox, kBox);
    const double y = qBound(0.0, (atPx.y() - r.top()) / r.height() * kBox, kBox);
    return QPointF(x, y);
}

void CanvasWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = rect();
    // Drawing-surface roles from the palette (the paper flag
    // picks the variant, like a theme would).
    const bool darkPaper = AppSettings::shared().resolvedIcons() == IconScheme::Light;
    const QColor paper = MaterialPalette::canvasPaper(darkPaper);
    const QColor gridColor = MaterialPalette::canvasGrid(darkPaper);
    const QColor helpColor = MaterialPalette::canvasHelp();
    const QColor inkColor = MaterialPalette::canvasInk(darkPaper);
    p.fillRect(r, paper);
    if (gridVisible) {
        p.setPen(QPen(gridColor, 1.0));
        for (double f : {0.1, 0.5, 0.9}) {
            const double x = r.left() + r.width() * f;
            const double y = r.top() + r.height() * f;
            p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
            p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
        }
    }

    const auto toPx = [&](const QPointF &q) {
        return QPointF(r.left() + q.x() / kBox * r.width(), r.top() + q.y() / kBox * r.height());
    };
    const auto poly = [&](const QVector<QPointF> &s, const QPen &pen) {
        if (s.size() < 2) {
            if (!s.isEmpty()) {
                p.setPen(Qt::NoPen);
                p.setBrush(pen.color());
                p.drawEllipse(toPx(s.first()), 2.5, 2.5);
            }
            return;
        }
        QPainterPath path(toPx(s.first()));
        for (int i = 1; i < s.size(); ++i) {
            path.lineTo(toPx(s.at(i)));
        }
        p.setPen(pen);
        p.drawPath(path);
    };

    if (finished != nullptr) {
        const QPen ink(inkColor, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        for (const auto &s : *finished) {
            poly(s, ink);
        }
    }
    if (open != nullptr && !open->isEmpty()) {
        const QPen active(MaterialPalette::activeStroke(), 3.0, Qt::SolidLine, Qt::RoundCap,
                          Qt::RoundJoin);
        poly(*open, active);
    }
    if ((finished == nullptr || finished->isEmpty()) && (open == nullptr || open->isEmpty())) {
        auto textRect = QRectF(r.left() + r.width() * .15, r.top() + r.height() * .15,
                               r.width() * .7, r.height() * .7);
        p.setPen(QPen(helpColor, 1.0));
        p.drawText(textRect, tr("Draw a Japanese character here and select a candidate! "
                                "(Right-click removes drawing)"));
    }
}

void CanvasWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit strokeStarted(to255(event->position()));
    } else if (event->button() == Qt::RightButton) {
        emit clearDrawing();
    }
}

void CanvasWidget::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        emit strokeMoved(to255(event->position()));
    }
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit strokeEnded();
    }
}
