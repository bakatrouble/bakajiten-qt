#include "TestMainWindow.h"

#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFont>
#include <QMenuBar>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QTimer>

#include "core/settings/Settings.h"
#include "core/settings/YamlFormat.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/settings/SettingsDialog.h"
#include "widgets/table/DictionaryTable.h"

#include <QHeaderView>
#include <memory>

void TestMainWindow::scaleRoundTrip() {
    AppSettings baseline;
    baseline.setScale(1.0);
    baseline.save();
    baseline.apply();
    const double basePoint = QApplication::font().pointSizeF();
    // Live font follows the applied value...
    AppSettings up;
    up.setScale(1.5);
    QVERIFY(up != baseline);
    up.apply();
    QVERIFY(qFuzzyCompare(QApplication::font().pointSizeF(), basePoint * 1.5));
    // ...and values round-trip through storage.
    up.save();
    QCOMPARE(AppSettings::fromStorage().scale(), 1.5);
    baseline.save();
    baseline.apply();
    QVERIFY(qFuzzyCompare(QApplication::font().pointSizeF(), basePoint));
}

void TestMainWindow::settingsDialogScaleRecreates() {
    // Normalize first (entry state depends on test order).
    AppSettings baseline;
    baseline.setScale(1.0);
    baseline.setStyle(QString());
    baseline.setIcons(IconScheme::Auto);
    baseline.save();
    AppSettings::shared().load();
    AppSettings::shared().apply();
    MainWindow::consumeRecreateRequested();
    const double base = QApplication::font().pointSizeF();
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *action = window->findChild<QAction *>("actionSettings");
    QVERIFY2(action != nullptr, "settings action missing");
    // Drive the modal through the real MainWindow Apply path: move
    // the scale slider and Apply. A scale change persists, applies
    // the font, and requests in-process recreation (the dialog and
    // window close; the main loop rebuilds). Results land in locals
    // (asserted after the modal returns, so a failure can never
    // leave the dialog open to hang the suite).
    bool sawDialog = false;
    QTimer::singleShot(500, [&] {
        auto *dialog = qobject_cast<SettingsDialog *>(QApplication::activeModalWidget());
        if (dialog == nullptr) {
            return;
        }
        sawDialog = true;
        auto *slider = dialog->findChild<QSlider *>("scaleSlider");
        auto *buttons = dialog->findChild<QDialogButtonBox *>();
        auto *apply = buttons != nullptr ? buttons->button(QDialogButtonBox::Apply) : nullptr;
        if (slider != nullptr && apply != nullptr) {
            slider->setValue(150);
            QTest::mouseClick(apply, Qt::LeftButton);
        }
    });
    action->trigger();
    // The scaled font must be live before the restore (this is the
    // production state the recreated window builds from).
    const double scaledFont = QApplication::font().pointSizeF();
    // Restore the entry state first (later tests must run at 100%):
    // the flag was consumed below, the font re-applied here.
    const bool recreated = MainWindow::consumeRecreateRequested();
    baseline.save();
    AppSettings::shared().load();
    AppSettings::shared().apply();
    MainWindow::consumeRecreateRequested();
    QVERIFY(sawDialog);
    QVERIFY(recreated);
    QCOMPARE(scaledFont, base * 1.5);
    QCOMPARE(QApplication::font().pointSizeF(), base);
}

void TestMainWindow::fontScaleRefitsLiveChrome() {
    // Fixed geometries follow a live interface-font change (no
    // recreation): toolbar buttons stay square and fitting, table
    // rows regrow, the menu bar relayouts.
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *prefix = window->findChild<QPushButton *>("prefixButton");
    QVERIFY2(prefix != nullptr, "prefix button missing");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *menubar = window->findChild<QMenuBar *>("menubar");
    QVERIFY2(menubar != nullptr, "menu bar missing");
    const int buttonSide = prefix->width();
    QCOMPARE(buttonSide, 40);
    const int rowHeight = table->verticalHeader()->defaultSectionSize();
    const int menuHeight = menubar->height();
    // Restored on scope exit (assert failures return early).
    struct FontGuard {
        QFont saved = QApplication::font();
        ~FontGuard() { QApplication::setFont(saved); }
    } guard;
    QVERIFY(guard.saved.pointSizeF() > 0.0);
    QFont big = guard.saved;
    big.setPointSizeF(big.pointSizeF() * 2.0);
    QApplication::setFont(big);
    QTRY_VERIFY_WITH_TIMEOUT(prefix->width() > buttonSide, 5000);
    QCOMPARE(prefix->width(), prefix->height());
    // Fits the doubled text (~56px), not a doubled box (80px).
    QVERIFY(prefix->width() < buttonSide * 2);
    QVERIFY(table->verticalHeader()->defaultSectionSize() > rowHeight);
    QTRY_VERIFY_WITH_TIMEOUT(menubar->height() > menuHeight, 5000);
}

void TestMainWindow::fontScaleRescalesTableFont() {
    // Production path at 150%: persist, apply, fresh window (the
    // same shape as the in-process recreation) — the results table
    // font follows the interface font (the delegate derives every
    // cell font from it per paint, so this pins the table text).
    AppSettings baseline;
    baseline.setScale(1.0);
    baseline.setStyle(QString());
    baseline.setIcons(IconScheme::Auto);
    baseline.save();
    AppSettings::shared().load();
    AppSettings::shared().apply();
    const double base = QApplication::font().pointSizeF();
    AppSettings scaled;
    scaled.setScale(1.5);
    scaled.setStyle(QString());
    scaled.setIcons(IconScheme::Auto);
    scaled.save();
    AppSettings::shared().load();
    AppSettings::shared().apply();
    struct FontGuard {
        QFont saved = QApplication::font();
        ~FontGuard() { QApplication::setFont(saved); }
    } guard;
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    QCOMPARE(table->font().pointSizeF(), base * 1.5);
    QFont heading = table->font();
    heading.setBold(true);
    heading.setPointSizeF(heading.pointSizeF() + 2.0);
    QCOMPARE(table->verticalHeader()->defaultSectionSize(),
             QFontMetrics(heading).lineSpacing() + 12);
    // Restore the entry state (later tests run at 100%).
    baseline.save();
    AppSettings::shared().load();
    AppSettings::shared().apply();
}

void TestMainWindow::settingsScalesBothFontModes() {
    // Pure scaling math (no globals): point fonts scale points,
    // pixel fonts scale pixels (their pointSizeF() is -1, where a
    // point scale would silently never apply).
    QFont points;
    points.setPointSizeF(9.0);
    QCOMPARE(AppSettings::scaledFont(points, 1.5).pointSizeF(), 13.5);
    QFont pixels;
    pixels.setPixelSize(12);
    const QFont grown = AppSettings::scaledFont(pixels, 1.5);
    QCOMPARE(grown.pixelSize(), 18);
    QCOMPARE(grown.pointSizeF(), -1.0);
}
