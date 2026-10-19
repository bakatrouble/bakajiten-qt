#include "RecognizerWidget.h"

#include "ui_RecognizerWidget.h"

#include "widgets/canvas/CanvasWidget.h"

#include <QListWidget>

/// Stroke editing: canvas capture into `strokes`/`open`, undo/redo
/// as plain vector moves. Every mutation repaints the canvas and
/// re-queries the candidate strip.
void RecognizerWidget::refreshCanvas() const { ui->canvas->setStrokeData(&strokes, &open); }

void RecognizerWidget::onCanvasStarted(const QPointF &at255) {
    open.clear();
    open.push_back(at255);
    refreshCanvas();
}

void RecognizerWidget::onCanvasMoved(const QPointF &at255) {
    if (!open.isEmpty()) {
        if ((open.last() - at255).manhattanLength() > 1.2) {
            open.push_back(at255);
            refreshCanvas();
        }
    }
}

void RecognizerWidget::onCanvasEnded() {
    if (!open.isEmpty()) {
        strokes.push_back(open);
        open.clear();
        redoStrokes.clear();
        refreshCanvas();
        updateStatus();
        onRecognize();
    }
}

void RecognizerWidget::onUndo() {
    if (!strokes.isEmpty()) {
        redoStrokes.push_back(strokes.takeLast());
    }
    open.clear();
    refreshCanvas();
    updateStatus();
    onRecognize();
}

void RecognizerWidget::onRedo() {
    if (!redoStrokes.isEmpty()) {
        strokes.push_back(redoStrokes.takeLast());
    }
    refreshCanvas();
    updateStatus();
    onRecognize();
}

void RecognizerWidget::onClear() {
    strokes.clear();
    open.clear();
    redoStrokes.clear();
    ui->candidatesList->clear();
    refreshCanvas();
    updateStatus();
}
