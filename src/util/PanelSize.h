#pragma once

#include <QSize>

/// Panel-size math for a square canvas (see RecognizerWidget).
/// The canvas takes all slack in both directions, so with fixed
/// chrome (toolbar width, strip/status heights, margins) it is
/// square exactly when (panelW - panelH) == inset, where inset =
/// chromeW - chromeH measured live. All pure: unit-tested in
/// `tests/test_panelsize.cpp`.
///
/// Grow and shrink are separate operations: growing preserves
/// minimums (but may leave the parent), shrinking preserves the
/// parent (but may break minimums). snapPanelSize orders them so
/// the result is square + in-parent always, and minimums hold
/// whenever the parent allows them (i.e. the line meets the box).

/// Grow the lagging dimension onto the w-h==inset line.
inline void snapSquareGrow(int &w, int &h, int inset) {
    if (w - h > inset) {
        h = w - inset;
    } else {
        w = h + inset;
    }
}

/// Shrink the leading dimension onto the line.
inline void snapSquareShrink(int &w, int &h, int inset) {
    if (w - h > inset) {
        w = h + inset;
    } else {
        h = w - inset;
    }
}

/// Full snap: minimums, square line, parent bounds, re-square.
inline QSize snapPanelSize(QSize req, QSize lo, QSize max, int inset) {
    int w = qMax(req.width(), lo.width());
    int h = qMax(req.height(), lo.height());
    snapSquareGrow(w, h, inset);
    w = qMin(w, max.width());
    h = qMin(h, max.height());
    snapSquareShrink(w, h, inset);
    return {w, h};
}

/// Chrome inset from live geometry.
inline int canvasInset(QSize panel, QSize canvas) {
    return (panel.width() - canvas.width()) - (panel.height() - canvas.height());
}
