#include "CanvasWidget.h"

#include "core/palette/MaterialPalette.h"
#include "core/settings/Settings.h"
#include "core/state/SettingsStore.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>

CanvasWidget::CanvasWidget(QWidget *parent) : QWidget(parent) {
    updateMinimum();
    // Plain expanding both ways: the panel clamps, never forces a
    // shape — an expanding height stretches to fill the row.
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

CanvasWidget::Grid CanvasWidget::gridFor(QSize size) {
    // As many min-size cells as fit (at least one each way), grown
    // uniformly to fill, block centered.
    const int cols = std::max(1, size.width() / minCellPx);
    const int rows = std::max(1, size.height() / minCellPx);
    const int cell = std::max(1, std::min(size.width() / cols, size.height() / rows));
    return {cols, rows, cell, (size.width() - cols * cell) / 2.0,
            (size.height() - rows * cell) / 2.0};
}

QPointF CanvasWidget::toPen(const QPointF &atPx) const {
    constexpr double kBox = 255.0;
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
    const bool darkPaper = SettingsStore::shared().current().darkPaper();
    const QColor paper = MaterialPalette::canvasPaper(darkPaper);
    const QColor gridColor = MaterialPalette::canvasGrid(darkPaper);
    const QColor helpColor = MaterialPalette::canvasHelp();
    const QColor inkColor = MaterialPalette::canvasInk(darkPaper);
    p.fillRect(r, paper);
    if (gridVisible) {
        // Uniform lattice (border included), refit every paint.
        const Grid grid = gridFor(r.size().toSize());
        p.setPen(QPen(gridColor, 1.0));
        for (int i = 0; i <= grid.cols; ++i) {
            const double x = grid.ox + i * grid.cell;
            p.drawLine(QPointF(x, grid.oy), QPointF(x, grid.oy + grid.rows * grid.cell));
        }
        for (int j = 0; j <= grid.rows; ++j) {
            const double y = grid.oy + j * grid.cell;
            p.drawLine(QPointF(grid.ox, y), QPointF(grid.ox + grid.cols * grid.cell, y));
        }
    }

    const auto poly = [&](const QVector<QPointF> &s, const QPen &pen) {
        if (s.size() < 2) {
            if (!s.isEmpty()) {
                p.setPen(Qt::NoPen);
                p.setBrush(pen.color());
                p.drawEllipse(s.first(), 2.5, 2.5);
            }
            return;
        }
        QPainterPath path(s.first());
        for (int i = 1; i < s.size(); ++i) {
            path.lineTo(s.at(i));
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
        emit strokeStarted(event->position());
    } else if (event->button() == Qt::RightButton) {
        emit clearDrawing();
    }
}

void CanvasWidget::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        emit strokeMoved(event->position());
    }
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit strokeEnded();
    }
}

void CanvasWidget::wheelEvent(QWheelEvent *event) {
    // Wheel steps through history (up = undo, down = redo), one
    // notch per step; high-res deltas accumulate toward it.
    const QPoint pixel = event->pixelDelta();
    const QPoint angle = event->angleDelta();
    const int delta = pixel.y() != 0   ? pixel.y()
                      : angle.y() != 0 ? angle.y()
                      : pixel.x() != 0 ? pixel.x()
                                       : angle.x();
    wheelAccum += delta;
    while (wheelAccum >= 120) {
        wheelAccum -= 120;
        emit undoRequested();
    }
    while (wheelAccum <= -120) {
        wheelAccum += 120;
        emit redoRequested();
    }
    event->accept();
}
