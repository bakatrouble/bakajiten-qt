#include <QtTest>

#include "util/PanelSize.h"

#include "TestPanelSize.h"

void TestPanelSize::squareLineHolds() {
    // Any request lands on the line when bounds allow.
    for (const QSize req : {QSize(400, 400), QSize(600, 300), QSize(300, 600), QSize(230, 258)}) {
        const QSize out = snapPanelSize(req, QSize(230, 258), QSize(2000, 2000), -70);
        QCOMPARE(out.width() - out.height(), -70);
    }
}

void TestPanelSize::minimumsGrowOntoLine() {
    // Below-minimum requests grow (never shrink below need).
    const QSize out = snapPanelSize(QSize(100, 100), QSize(230, 258), QSize(2000, 2000), -70);
    QVERIFY(out.width() >= 230);
    QVERIFY(out.height() >= 258);
    QCOMPARE(out.width() - out.height(), -70);
}

void TestPanelSize::parentClampKeepsSquare() {
    // A grown size past the parent comes back square and inside.
    const QSize out = snapPanelSize(QSize(900, 900), QSize(230, 258), QSize(500, 500), -70);
    QCOMPARE(out.width() - out.height(), -70);
    QVERIFY(out.width() <= 500);
    QVERIFY(out.height() <= 500);
    QVERIFY(out.width() >= 230);
    QVERIFY(out.height() >= 258);
}

void TestPanelSize::degenerateParentKeepsSquare() {
    // Parent smaller than content: squareness still wins (minimums
    // may give — nothing fits anyway).
    const QSize out = snapPanelSize(QSize(500, 500), QSize(230, 258), QSize(100, 100), -70);
    QCOMPARE(out.width() - out.height(), -70);
    QVERIFY(out.width() <= 100);
    QVERIFY(out.height() <= 100);
}

void TestPanelSize::insetSignVariants() {
    // Zero and positive insets behave the same way.
    const QSize z = snapPanelSize(QSize(400, 500), QSize(100, 100), QSize(2000, 2000), 0);
    QCOMPARE(z.width(), z.height());
    const QSize p = snapPanelSize(QSize(400, 500), QSize(100, 100), QSize(2000, 2000), 60);
    QCOMPARE(p.width() - p.height(), 60);
    // canvasInset: (panel - canvas) per dimension, differenced.
    QCOMPARE(canvasInset(QSize(500, 570), QSize(450, 450)), -70);
    QCOMPARE(canvasInset(QSize(230, 258), QSize(200, 200)), -28);
}
