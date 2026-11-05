#include <QtTest>

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalSpy>
#include <QWheelEvent>

#include "TestDataDir.h"
#include "core/settings/Settings.h"
#include "ui/recognizer/RecognizerWidget.h"
#include "widgets/canvas/CanvasWidget.h"
#include "widgets/list/CandidateList.h"
#include "widgets/list/RecognizerCandidateList.h"

#include "TestPanel.h"

namespace {
/// Panel hosted in a plain fixed parent (stands in for the main
/// window central widget), shown and exposed. The widget sizes
/// itself (persisted size, else the default), so no resize here.
RecognizerWidget *panelIn(QWidget &parent) {
    auto *panel = new RecognizerWidget(&parent);
    panel->move(50, 50);
    panel->show();
    parent.resize(1100, 750);
    parent.show();
    if (!QTest::qWaitForWindowExposed(&parent)) {
        return nullptr;
    }
    return panel;
}

QSize canvasSize(RecognizerWidget *panel) {
    auto *canvas = panel->findChild<QWidget *>("canvas");
    if (canvas == nullptr) {
        return QSize();
    }
    return canvas->size();
}

/// Drag the resize grip by (dx, dy) via real mouse events.
void dragGrip(RecognizerWidget *panel, int dx, int dy) {
    auto *grip = panel->findChild<QLabel *>("resizeGrip");
    QVERIFY2(grip != nullptr, "resize grip missing");
    const QPoint at = grip->rect().center();
    QTest::mousePress(grip, Qt::LeftButton, Qt::NoModifier, at);
    QTest::mouseMove(grip, at + QPoint(dx, dy));
    QTest::mouseRelease(grip, Qt::LeftButton, Qt::NoModifier, at + QPoint(dx, dy));
}
} // namespace

void TestRecognizerPanel::initTestCase() {
    // Isolate from the developer's real config: memorized UI
    // values (panel size) live in a temp-adjacent state file,
    // AppSettings (YAML) points at a temp file.
    QCoreApplication::setOrganizationName(QStringLiteral("bakajiten-test"));
    QCoreApplication::setApplicationName(QStringLiteral("bakajiten-test"));
    QFile::remove(AppSettings::memorizedPath());
    qputenv("BAKAJITEN_SETTINGS",
            (QDir::tempPath() + QStringLiteral("/bakajiten-test-settings.yaml")).toUtf8());
    pointDataDirAtRepoData();
    AppSettings::resetStorage();
}

void TestRecognizerPanel::cleanupTestCase() {
    QFile::remove(AppSettings::memorizedPath());
    AppSettings::resetStorage();
}

void TestRecognizerPanel::canvasHasGeometryOnFirstOpen() {
    QFile::remove(AppSettings::memorizedPath());
    QWidget window;
    window.resize(1100, 750);
    // App flow: the panel is built hidden while the main window is
    // still coming up; only the open click shows it.
    auto *panel = new RecognizerWidget(&window);
    panel->move(50, 50);
    QApplication::processEvents();
    QTest::qWait(50);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    panel->show();
    QTest::qWait(100);
    const QSize canvas = canvasSize(panel);
    QVERIFY2(!canvas.isNull(), "canvas child missing");
    QVERIFY2(!canvas.isEmpty(), "canvas has no geometry");
}

void TestRecognizerPanel::stripDefaultsHorizontal() {
    // Bare construction strips horizontally (the contract used to
    // live in the panel .ui — dialog instances listed vertically).
    CandidateList strip;
    QCOMPARE(strip.flow(), QListView::LeftToRight);
    QVERIFY(!strip.isWrapping());
    QCOMPARE(strip.verticalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    QCOMPARE(strip.horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    QCOMPARE(strip.sizePolicy().verticalPolicy(), QSizePolicy::Fixed);
    QCOMPARE(strip.minimumHeight(), 40);
    QCOMPARE(strip.maximumHeight(), 40);
    QVERIFY(strip.uniformItemSizes());
}

void TestRecognizerPanel::stripCloneUsesSingleLineRows() {
    // Exclusive strip: roomy rows, free item sizes, no grid.
    RecognizerCandidateList strip;
    QCOMPARE(strip.minimumHeight(), 20);
    QCOMPARE(strip.maximumHeight(), 20);
    QVERIFY(!strip.uniformItemSizes());
    QVERIFY(!strip.gridSize().isValid());
    for (int i = 0; i < 5; ++i) {
        strip.addItem(QString("item %1").arg(i));
    }
    strip.resize(400, 20);
    QApplication::processEvents();
    // Items size to their text: wider than tall, never
    // square-forced.
    const QRect cell = strip.visualRect(strip.model()->index(0, 0));
    QVERIFY2(cell.width() > cell.height(), "strip cell is not content-sized");
    // Live scale: rows track single line heights, so the clone
    // row stays under the shared strip's 1.4x row.
    CandidateList base;
    RecognizerCandidateList rec;
    base.refreshScale();
    rec.refreshScale();
    QVERIFY2(rec.minimumHeight() < base.minimumHeight(), "clone row is not single-line");
    QCOMPARE(rec.minimumHeight(), rec.maximumHeight());
}

void TestRecognizerPanel::stripDragScrolls() {
    QWidget window;
    RecognizerWidget *panel = panelIn(window);
    QVERIFY2(panel != nullptr, "offscreen window never exposed");
    auto *list = panel->findChild<QListWidget *>("candidatesList");
    QVERIFY2(list != nullptr, "candidates list missing");
    for (int i = 0; i < 30; ++i) {
        list->addItem(QString("item %1").arg(i));
    }
    QApplication::processEvents();
    auto *bar = list->horizontalScrollBar();
    QVERIFY(bar->maximum() > 0);
    bar->setValue(bar->maximum());
    QApplication::processEvents();
    // Press, drag right past the threshold, release: the content
    // follows the cursor (bar moves back), and it is a drag, not a
    // click (no selection accepted).
    auto *viewport = list->viewport();
    const QPoint at = viewport->rect().center();
    QMouseEvent press(QEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(viewport, &press);
    QMouseEvent move(QEvent::MouseMove, at + QPoint(60, 0), Qt::NoButton, Qt::LeftButton,
                     Qt::NoModifier);
    QApplication::sendEvent(viewport, &move);
    QMouseEvent release(QEvent::MouseButtonRelease, at + QPoint(60, 0), Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(viewport, &release);
    QVERIFY(bar->value() < bar->maximum());
}

void TestRecognizerPanel::stripNeverScrollsVertically() {
    QWidget window;
    RecognizerWidget *panel = panelIn(window);
    QVERIFY2(panel != nullptr, "offscreen window never exposed");
    auto *list = panel->findChild<QListWidget *>("candidatesList");
    QVERIFY2(list != nullptr, "candidates list missing");
    for (int i = 0; i < 30; ++i) {
        list->addItem(QString("item %1").arg(i));
    }
    QApplication::processEvents();
    auto *vertical = list->verticalScrollBar();
    auto *bar = list->horizontalScrollBar();
    QVERIFY(bar->maximum() > 0);
    bar->setValue(bar->maximum());
    QApplication::processEvents();
    // Vertical wheel glides the strip horizontally instead (animated,
    // so content never jumps — or moves vertically).
    const QPoint at = list->viewport()->rect().center();
    QWheelEvent wheel(at, list->viewport()->mapToGlobal(at), QPoint(), QPoint(0, 120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(list->viewport(), &wheel);
    QCOMPARE(vertical->value(), 0);
    QCOMPARE(bar->value(), bar->maximum());
    QTest::qWait(300);
    QVERIFY(bar->value() < bar->maximum());
    // Cells fit the viewport: no vertical range exists at all.
    QCOMPARE(vertical->maximum(), 0);
    QVERIFY(list->visualRect(list->model()->index(0, 0)).height() <= list->viewport()->height());
}

void TestRecognizerPanel::cornersStayTransparent() {
    QWidget window;
    RecognizerWidget *panel = panelIn(window);
    QVERIFY2(panel != nullptr, "offscreen window never exposed");
    panel->show();
    QApplication::processEvents();
    QTest::qWait(100);
    const QImage shot = panel->grab().toImage();
    QVERIFY2(shot.hasAlphaChannel(), "grab has no alpha channel");
    // Window corners sit outside the rounded card: no opaque fill,
    // only the soft shadow's faint falloff.
    QVERIFY2(shot.pixelColor(0, 0).alpha() < 32, "top-left corner is opaque");
    QVERIFY2(shot.pixelColor(shot.width() - 1, 0).alpha() < 32, "top-right corner is opaque");
    QVERIFY2(shot.pixelColor(0, shot.height() - 1).alpha() < 32, "bottom-left corner is opaque");
    QVERIFY2(shot.pixelColor(shot.width() - 1, shot.height() - 1).alpha() < 32,
             "bottom-right corner is opaque");
}

void TestRecognizerPanel::resizeGripShowsTriangle() {
    QWidget window;
    RecognizerWidget *panel = panelIn(window);
    QVERIFY2(panel != nullptr, "offscreen window never exposed");
    auto *grip = panel->findChild<QLabel *>("resizeGrip");
    QVERIFY2(grip != nullptr, "resize grip missing");
    QApplication::processEvents();
    // Centroid of the painted triangle (right angle at the grip's
    // bottom-right corner, full legs): text-color fill.
    const QPoint origin = grip->mapTo(panel, QPoint(0, 0));
    const int leg = qMin(grip->width(), grip->height());
    const QPoint centroid(origin.x() + grip->width() - leg / 3,
                          origin.y() + grip->height() - leg / 3);
    QCOMPARE(panel->grab().toImage().pixelColor(centroid).rgb(),
             panel->palette().color(QPalette::WindowText).rgb());
}

void TestRecognizerPanel::canvasGridFitsCells() {
    // Uniform square cells, never under the minimum: counts grow
    // exactly when another cell fits, block centered.
    using Grid = CanvasWidget::Grid;
    const Grid full = CanvasWidget::gridFor(QSize(400, 300));
    QCOMPARE(full.cols, 10);
    QCOMPARE(full.rows, 7);
    QCOMPARE(full.cell, 40);
    QCOMPARE(full.ox, 0.0);
    QCOMPARE(full.oy, 10.0);
    // The count flips the moment a full cell fits.
    QCOMPARE(CanvasWidget::gridFor(QSize(399, 300)).cols, 9);
    QCOMPARE(CanvasWidget::gridFor(QSize(400, 300)).cols, 10);
    QCOMPARE(CanvasWidget::gridFor(QSize(400, 279)).rows, 6);
    QCOMPARE(CanvasWidget::gridFor(QSize(400, 280)).rows, 7);
    // Leftover space centers the block (cells stay uniform).
    const Grid wide = CanvasWidget::gridFor(QSize(430, 300));
    QCOMPARE(wide.cols, 10);
    QCOMPARE(wide.cell, 42);
    QCOMPARE(wide.ox, 5.0);
    // Degenerate sizes still yield one cell, never zero.
    const Grid tiny = CanvasWidget::gridFor(QSize(30, 30));
    QCOMPARE(tiny.cols, 1);
    QCOMPARE(tiny.rows, 1);
    QCOMPARE(tiny.cell, 30);
    // Painted lattice: a grid-line pixel differs from paper, an
    // interior pixel does not carry the line ink.
    QWidget window;
    auto *canvas = new CanvasWidget(&window);
    canvas->resize(400, 300);
    window.show();
    canvas->show();
    QApplication::processEvents();
    QTest::qWait(100);
    const QImage shot = canvas->grab().toImage();
    // First interior vertical line lands at ox + cell.
    const QColor line = shot.pixelColor(40, 150);
    const QColor paper = shot.pixelColor(60, 150);
    QVERIFY(line != paper);
}

void TestRecognizerPanel::resizeKeepsInkUnscaled() {
    // Drawn ink lives in canvas pixels: enlarging the panel must
    // neither move nor scale it. Grid off, so the overlap is
    // paper + ink only and compares pixel-exact.
    QWidget window;
    RecognizerWidget *panel = panelIn(window);
    QVERIFY2(panel != nullptr, "offscreen window never exposed");
    auto *grid = panel->findChild<QPushButton *>("gridButton");
    QVERIFY2(grid != nullptr, "grid button missing");
    QTest::mouseClick(grid, Qt::LeftButton);
    auto *canvas = panel->findChild<QWidget *>("canvas");
    QVERIFY2(canvas != nullptr, "canvas missing");
    const QPoint from(40, 40);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, from);
    QTest::mouseMove(canvas, from + QPoint(60, 0));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, from + QPoint(60, 0));
    QApplication::processEvents();
    const QImage before = canvas->grab().toImage();
    dragGrip(panel, 120, 80);
    QApplication::processEvents();
    const QImage after = canvas->grab().toImage();
    QVERIFY2(after.width() >= before.width() && after.height() >= before.height(),
             "canvas did not grow with the panel");
    QCOMPARE(after.copy(0, 0, before.width(), before.height()), before);
}

void TestRecognizerPanel::gripDragClampsAtParentBounds() {
    // Parked near the parent's bottom-right, a big drag clamps
    // into the parent instead of overflowing it.
    QWidget window;
    RecognizerWidget *panel = panelIn(window);
    QVERIFY2(panel != nullptr, "offscreen window never exposed");
    panel->move(700, 500);
    QApplication::processEvents();
    const QPoint at = panel->pos();
    dragGrip(panel, 400, 300);
    // Width clamps at the parent edge; height hits the layout
    // floor first (it is taller than the parent remainder).
    QCOMPARE(panel->size().width(), 1100 - 700);
    QCOMPARE(panel->size().height(), panel->layout()->minimumSize().height());
    QCOMPARE(panel->pos(), at);
}

void TestRecognizerPanel::wheelOnCanvasUndoesAndRedoes() {
    QWidget window;
    RecognizerWidget *panel = panelIn(window);
    QVERIFY2(panel != nullptr, "offscreen window never exposed");
    auto *canvas = panel->findChild<QWidget *>("canvas");
    QVERIFY2(canvas != nullptr, "canvas missing");
    auto *status = panel->findChild<QLabel *>("statusLabel");
    QVERIFY2(status != nullptr, "status label missing");
    // One stroke through the real mouse path.
    const QPoint from(40, 40);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, from);
    QTest::mouseMove(canvas, from + QPoint(60, 0));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, from + QPoint(60, 0));
    QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("1 stroke")), 5000);
    // Wheel up steps back through history, wheel down forward.
    const QPoint at = canvas->rect().center();
    QWheelEvent up(at, canvas->mapToGlobal(at), QPoint(), QPoint(0, 120), Qt::NoButton,
                   Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(canvas, &up);
    QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("0 stroke")), 5000);
    QWheelEvent down(at, canvas->mapToGlobal(at), QPoint(), QPoint(0, -120), Qt::NoButton,
                     Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(canvas, &down);
    QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("1 stroke")), 5000);
}

void TestRecognizerPanel::gripDragResizesFreely() {
    QWidget window;
    RecognizerWidget *panel = panelIn(window);
    QVERIFY2(panel != nullptr, "offscreen window never exposed");
    const QSize before = panel->size();
    const QPoint at = panel->pos();
    dragGrip(panel, 120, 80);
    // Free resize: the drag lands unclamped and unforced (no
    // square snapping) inside the 1100x750 parent — and the panel
    // does not wander (popup resize commits can nudge it).
    QCOMPARE(panel->size(), before + QSize(120, 80));
    QCOMPARE(panel->pos(), at);
    dragGrip(panel, -200, 200);
    const QSize second = panel->size();
    QVERIFY2(second != before + QSize(120, 80), "second drag changed nothing");
    QVERIFY2(!canvasSize(panel).isEmpty(), "canvas has no geometry after free resize");
}

void TestRecognizerPanel::sizePersistsAcrossRecreation() {
    QFile::remove(AppSettings::memorizedPath());
    QSize grown;
    {
        QWidget window;
        RecognizerWidget *panel = panelIn(window);
        QVERIFY2(panel != nullptr, "offscreen window never exposed");
        const QSize before = panel->size();
        dragGrip(panel, 120, 80);
        grown = panel->size();
        QVERIFY2(grown != before, "grip drag changed nothing");
    }
    {
        QWidget window;
        RecognizerWidget *panel = panelIn(window);
        QVERIFY2(panel != nullptr, "offscreen window never exposed");
        QTRY_COMPARE(panel->size(), grown);
    }
    QFile::remove(AppSettings::memorizedPath());
}

void TestRecognizerPanel::canvasFollowsTheme() {
    // No icon scheme: canvas paper tracks the live theme palette
    // (dark palette = dark paper, light = light), re-read every
    // repaint. Grid off: the center pixel must be pure surface.
    QWidget window;
    auto *canvas = new CanvasWidget(&window);
    canvas->setGridVisible(false);
    window.resize(400, 400);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    const QPalette saved = QApplication::palette();
    QPalette dark;
    dark.setColor(QPalette::Window, QColor(30, 30, 30));
    QApplication::setPalette(dark);
    const QColor darkPaper = canvas->grab().toImage().pixelColor(canvas->rect().center());
    QApplication::setPalette(saved);
    const QColor lightPaper = canvas->grab().toImage().pixelColor(canvas->rect().center());
    QVERIFY2(darkPaper.lightnessF() < 0.2, "dark theme must draw dark paper");
    QVERIFY2(lightPaper.lightnessF() > 0.9, "light theme must draw light paper");
}
