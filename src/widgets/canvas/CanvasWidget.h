#pragma once

#include <QPointF>
#include <QVector>
#include <QWidget>

/// Stroke-capture canvas: each mouse drag is one stroke. Paints the
/// finished strokes plus the one in progress; geometry is the
/// recognizer's pen-space mapped onto the widget rect.
///
/// Instantiated by Designer (promoted widget in `RecognizerWidget.ui`),
/// never by hand.
class CanvasWidget : public QWidget {
    Q_OBJECT
public:
    explicit CanvasWidget(QWidget *parent = nullptr);

    void setStrokeData(const QVector<QVector<QPointF>> *finished, const QVector<QPointF> *open);

    /// Show or hide the pen-space grid (display only).
    void setGridVisible(bool visible);

signals:
    void strokeStarted(const QPointF &at255);
    void strokeMoved(const QPointF &at255);
    void strokeEnded();
    void clearDrawing();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    /// Font changes (e.g. interface scale) re-derive the minimum
    /// from the widget's own font — no settings coupling.
    void changeEvent(QEvent *event) override;

private:
    QPointF to255(const QPointF &atPx) const;
    /// Minimum side from the widget's own font (~220px at 9pt).
    void updateMinimum();

    const QVector<QVector<QPointF>> *finished = nullptr;
    const QVector<QPointF> *open = nullptr;
    bool gridVisible = true;
};
