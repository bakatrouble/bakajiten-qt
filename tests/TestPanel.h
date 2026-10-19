#pragma once

#include <QObject>

/// Panel-level guarantees: whatever the drag requests, the canvas
/// comes out square; the size persists across recreations. Drives
/// the real grip through QTest mouse events (eventFilter →
/// applyPanelSize → layout), offscreen. No model is needed:
/// geometry never touches the recognizer.
class TestRecognizerPanel : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void canvasSquareAfterWideDrag();
    void canvasSquareAfterTallDrag();
    void canvasSquareOnFirstOpen();
    void stripNeverScrollsVertically();
    void stripDefaultsHorizontal();
    void stripDragScrolls();
    void resizeGripShowsTriangle();
    void sizePersistsAcrossRecreation();
    void canvasFollowsIconScheme();
};
