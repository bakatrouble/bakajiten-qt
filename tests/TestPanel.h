#pragma once

#include <QObject>

/// Panel-level guarantees: the grip resizes freely (no shape
/// forcing, clamped to layout minimum and parent), the size
/// persists across recreations, and the recognizer strip runs
/// double-size type with content-sized cells. Drives the real grip through QTest mouse events (eventFilter →
/// applyPanelSize → layout), offscreen. No model is needed:
/// geometry never touches the recognizer.
class TestRecognizerPanel : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void gripDragResizesFreely();
    void gripDragClampsAtParentBounds();
    void wheelOnCanvasUndoesAndRedoes();
    void canvasHasGeometryOnFirstOpen();
    void stripNeverScrollsVertically();
    void stripDefaultsHorizontal();
    void stripCloneUsesSingleLineRows();
    void canvasGridFitsCells();
    void resizeKeepsInkUnscaled();
    void stripDragScrolls();
    void resizeGripShowsTriangle();
    void cornersStayTransparent();
    void sizePersistsAcrossRecreation();
    void canvasFollowsTheme();
};
