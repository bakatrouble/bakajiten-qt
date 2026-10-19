#pragma once

#include <QObject>

/// Unit tests for the square-canvas panel math: the canvas takes all
/// slack, so it is square exactly on the w-h==inset line. No widgets,
/// no pixels — pure QSize in/out.
class TestPanelSize : public QObject {
    Q_OBJECT

private slots:
    void squareLineHolds();
    void minimumsGrowOntoLine();
    void parentClampKeepsSquare();
    void degenerateParentKeepsSquare();
    void insetSignVariants();
};
