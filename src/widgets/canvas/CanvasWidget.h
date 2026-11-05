#pragma once

#include <QPointF>
#include <QVector>
#include <QWidget>

class QWheelEvent;

/// Stroke-capture canvas: each mouse drag is one stroke. Points
/// are canvas pixels — resizing never rescales drawn ink; they are
/// converted to the recognizer's pen space at query time.
/// Paints the finished strokes plus the one in progress.
///
/// The grid is uniform: square cells no smaller than
/// `minCellPx`, with the column/row counts refit every paint —
/// a new row or column appears as soon as another cell fits,
/// and the block stays centered.
///
/// Instantiated by Designer (promoted widget in `RecognizerWidget.ui`),
/// never by hand.
class CanvasWidget : public QWidget {
    Q_OBJECT
public:
    /// Minimum grid cell side in px.
    static constexpr int minCellPx = 40;

    /// Grid lattice for a canvas size: counts, uniform cell, and
    /// the centered block origin (pure: unit-testable).
    struct Grid {
        int cols = 0;
        int rows = 0;
        int cell = 0;
        double ox = 0.0;
        double oy = 0.0;
    };
    static Grid gridFor(QSize size);
    explicit CanvasWidget(QWidget *parent = nullptr);

    void setStrokeData(const QVector<QVector<QPointF>> *finished, const QVector<QPointF> *open);

    /// Show or hide the grid (display only).
    void setGridVisible(bool visible);

    /// Pixels → recognizer pen space (0..255) in the live geometry.
    QPointF toPen(const QPointF &atPx) const;

signals:
    void strokeStarted(const QPointF &atPx);
    void strokeMoved(const QPointF &atPx);
    void strokeEnded();
    void clearDrawing();
    /// Wheel stepped past one notch (up = undo, down = redo).
    void undoRequested();
    void redoRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    /// Wheel steps through history (up = undo, down = redo).
    void wheelEvent(QWheelEvent *event) override;
    /// Font changes (e.g. interface scale) re-derive the minimum
    /// from the widget's own font — no settings coupling.
    void changeEvent(QEvent *event) override;

private:
    /// Minimum side from the widget's own font (~220px at 9pt).
    void updateMinimum();

    const QVector<QVector<QPointF>> *finished = nullptr;
    const QVector<QPointF> *open = nullptr;
    bool gridVisible = true;
    /// High-res wheel remainder toward the next history step.
    int wheelAccum = 0;
};
