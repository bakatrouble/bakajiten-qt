#include <QtTest>

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSignalSpy>
#include <QWheelEvent>

#include "TestDataDir.h"
#include "core/settings/Settings.h"
#include "ui/recognizer/RecognizerWidget.h"
#include "widgets/canvas/CanvasWidget.h"
#include "widgets/list/CandidateList.h"

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

void TestRecognizerPanel::canvasSquareOnFirstOpen() {
    QFile::remove(AppSettings::memorizedPath());
    QWidget window;
    window.resize(1100, 750);
    // App flow: the panel is built hidden while the main window is
    // still coming up; only the open click shows it. Snapping at
    // build time would measure unlaid-out chrome.
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
    QCOMPARE(canvas.width(), canvas.height());
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

void TestRecognizerPanel::canvasSquareAfterWideDrag() {
    QWidget window;
    RecognizerWidget *panel = panelIn(window);
    QVERIFY2(panel != nullptr, "offscreen window never exposed");
    dragGrip(panel, 200, -200);
    const QSize canvas = canvasSize(panel);
    QVERIFY2(!canvas.isNull(), "canvas child missing");
    QVERIFY2(!canvas.isEmpty(), "canvas has no geometry");
    QCOMPARE(canvas.width(), canvas.height());
}

void TestRecognizerPanel::canvasSquareAfterTallDrag() {
    QWidget window;
    RecognizerWidget *panel = panelIn(window);
    QVERIFY2(panel != nullptr, "offscreen window never exposed");
    dragGrip(panel, -200, 200);
    const QSize canvas = canvasSize(panel);
    QVERIFY2(!canvas.isNull(), "canvas child missing");
    QVERIFY2(!canvas.isEmpty(), "canvas has no geometry");
    QCOMPARE(canvas.width(), canvas.height());
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

void TestRecognizerPanel::canvasFollowsIconScheme() {
    // The canvas reads the live shared values (mirroring the accept
    // path, which syncs them); storage round-trips are pinned
    // elsewhere. Grid off: the center pixel must be pure surface,
    // tracking the scheme (dark icons sit on light and vice versa).
    AppSettings dark;
    dark.setIcons(IconScheme::Dark);
    dark.save();
    AppSettings::shared() = dark;
    QWidget window;
    auto *canvas = new CanvasWidget(&window);
    canvas->setGridVisible(false);
    window.resize(400, 400);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    const QColor darkSurface = canvas->grab().toImage().pixelColor(canvas->rect().center());
    QVERIFY2(darkSurface.lightnessF() > 0.9, "dark icons must sit on a light surface");
    AppSettings light;
    light.setIcons(IconScheme::Light);
    light.save();
    AppSettings::shared() = light;
    const QColor lightSurface = canvas->grab().toImage().pixelColor(canvas->rect().center());
    QVERIFY2(lightSurface.lightnessF() < 0.2, "light icons must sit on a dark surface");
    QVERIFY(darkSurface != lightSurface);
    AppSettings automatic;
    automatic.setIcons(IconScheme::Auto);
    automatic.save();
    AppSettings::shared() = automatic;
}
