#include "TestMainWindow.h"

#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QSignalSpy>
#include <QSlider>
#include <QStackedWidget>
#include <QTimer>

#include "core/settings/Settings.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/settings/SettingsDialog.h"
#include "util/Romaji.h"

#include <QPushButton>
#include <memory>

void TestMainWindow::settingsDialogDirtyTracking() {
    // Normalize all persisted values first (entry state depends on
    // test order).
    AppSettings baseline;
    baseline.setScale(1.0);
    baseline.setStyle(QString());
    baseline.setIcons(IconScheme::Auto);
    baseline.save();
    SettingsDialog dialog(nullptr);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    auto *buttons = dialog.findChild<QDialogButtonBox *>();
    QVERIFY2(buttons != nullptr, "button box missing");
    auto *apply = buttons->button(QDialogButtonBox::Apply);
    QVERIFY2(apply != nullptr, "Apply button missing");
    auto *warning = dialog.findChild<QLabel *>("scaleWarning");
    QVERIFY2(warning != nullptr, "scale warning missing");
    // Clean open: Apply disabled, notice hidden.
    QVERIFY(!apply->isEnabled());
    QVERIFY(!warning->isVisibleTo(&dialog));
    // Scale dirties (and warns: only scale recreates the window).
    auto *slider = dialog.findChild<QSlider *>();
    QVERIFY2(slider != nullptr, "scale slider missing");
    slider->setValue(150);
    QVERIFY(apply->isEnabled());
    QVERIFY(warning->isVisibleTo(&dialog));
    slider->setValue(100);
    QVERIFY(!apply->isEnabled());
    QVERIFY(!warning->isVisibleTo(&dialog));
    // Slider stops at 25% steps: in-between values snap.
    slider->setValue(137);
    QCOMPARE(slider->value(), 125);
    QCOMPARE(dialog.selectedScale(), 1.25);
    slider->setValue(100);
    // Icons dirties without warning (applies live); restoring cleans.
    auto *icons = dialog.findChild<QComboBox *>("iconsCombo");
    QVERIFY2(icons != nullptr, "icons combo missing");
    const int dark = icons->findData(static_cast<int>(IconScheme::Dark));
    QVERIFY2(dark >= 0, "Dark icons missing");
    const int automatic = icons->findData(static_cast<int>(IconScheme::Auto));
    icons->setCurrentIndex(dark);
    QVERIFY(apply->isEnabled());
    QVERIFY(!warning->isVisibleTo(&dialog));
    icons->setCurrentIndex(automatic);
    QVERIFY(!apply->isEnabled());
    // Theme dirties without warning; restoring cleans.
    auto *theme = dialog.findChild<QComboBox *>("themeCombo");
    QVERIFY2(theme != nullptr, "theme combo missing");
    QVERIFY2(theme->count() >= 2, "no installed styles to select");
    theme->setCurrentIndex(1);
    QVERIFY(apply->isEnabled());
    QVERIFY(!warning->isVisibleTo(&dialog));
    theme->setCurrentIndex(0);
    QVERIFY(!apply->isEnabled());
}

void TestMainWindow::settingsDialogApplyUnDirties() {
    // Normalize first (entry state depends on test order).
    AppSettings baseline;
    baseline.setScale(1.0);
    baseline.setStyle(QString());
    baseline.setIcons(IconScheme::Auto);
    baseline.save();
    AppSettings::shared().load();
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *action = window->findChild<QAction *>("actionSettings");
    QVERIFY2(action != nullptr, "settings action missing");
    // Drive the modal through the real MainWindow Apply path: dirty
    // it with a live-applied setting (icons need no recreation, so
    // the dialog stays open), Apply, and read the dirt back. Results
    // land in locals (asserted after the modal returns, so a failure
    // can never leave the dialog open to hang the suite).
    bool sawDialog = false;
    bool wasEnabled = false;
    bool persisted = false;
    bool clean = false;
    bool restored = false;
    QTimer::singleShot(500, [&] {
        auto *dialog = qobject_cast<SettingsDialog *>(QApplication::activeModalWidget());
        if (dialog == nullptr) {
            return;
        }
        sawDialog = true;
        auto *icons = dialog->findChild<QComboBox *>("iconsCombo");
        auto *buttons = dialog->findChild<QDialogButtonBox *>();
        auto *apply = buttons != nullptr ? buttons->button(QDialogButtonBox::Apply) : nullptr;
        const int dark =
            icons != nullptr ? icons->findData(static_cast<int>(IconScheme::Dark)) : -1;
        const int automatic =
            icons != nullptr ? icons->findData(static_cast<int>(IconScheme::Auto)) : -1;
        if (icons != nullptr && apply != nullptr && dark >= 0 && automatic >= 0) {
            icons->setCurrentIndex(dark);
            wasEnabled = apply->isEnabled();
            QTest::mouseClick(apply, Qt::LeftButton);
            // Persisted through the MainWindow handler, dialog re-snapped.
            persisted = AppSettings::fromStorage().icons() == IconScheme::Dark;
            clean = !dialog->isDirty() && !apply->isEnabled();
            // Restore the entry state the same way (keeps later tests
            // hermetic), then close.
            icons->setCurrentIndex(automatic);
            QTest::mouseClick(apply, Qt::LeftButton);
            restored = AppSettings::fromStorage().icons() == IconScheme::Auto;
        }
        dialog->reject();
    });
    action->trigger();
    // Back from the modal: nothing recreated (icons apply live).
    QVERIFY(sawDialog);
    QVERIFY(wasEnabled);
    QVERIFY(persisted);
    QVERIFY(clean);
    QVERIFY(restored);
    QCOMPARE(AppSettings::fromStorage().icons(), IconScheme::Auto);
}

void TestMainWindow::settingsDialogReportsScale() {
    // Normalize first (entry state depends on test order).
    AppSettings baseline;
    baseline.setScale(1.0);
    baseline.setStyle(QString());
    baseline.setIcons(IconScheme::Auto);
    baseline.save();
    SettingsDialog dialog(nullptr);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    // Categories wired to their pages, in order.
    auto *categories = dialog.findChild<QListWidget *>("categoryList");
    QVERIFY2(categories != nullptr, "category list missing");
    QCOMPARE(categories->count(), 2);
    QCOMPARE(categories->item(0)->text(), QStringLiteral("Appearance"));
    QCOMPARE(categories->item(1)->text(), QStringLiteral("Updates"));
    auto *stack = dialog.findChild<QStackedWidget *>("settingsStack");
    QVERIFY2(stack != nullptr, "settings stack missing");
    QCOMPARE(stack->currentIndex(), 0);
    // Untouched: current factor, no warning.
    QCOMPARE(dialog.selectedScale(), 1.0);
    auto *warning = dialog.findChild<QLabel *>("scaleWarning");
    QVERIFY2(warning != nullptr, "scale warning missing");
    QVERIFY(!warning->isVisibleTo(&dialog));
    // Moved: new factor, warning shown. The dialog only reports —
    // Apply signals out (MainWindow persists + recreates) without
    // persisting or closing here.
    auto *slider = dialog.findChild<QSlider *>();
    QVERIFY2(slider != nullptr, "scale slider missing");
    slider->setValue(150);
    QCOMPARE(dialog.selectedScale(), 1.5);
    QVERIFY(warning->isVisibleTo(&dialog));
    auto *buttons = dialog.findChild<QDialogButtonBox *>();
    QVERIFY2(buttons != nullptr, "button box missing");
    auto *apply = buttons->button(QDialogButtonBox::Apply);
    QVERIFY2(apply != nullptr, "Apply button missing");
    QSignalSpy applied(&dialog, &SettingsDialog::applyRequested);
    QTest::mouseClick(apply, Qt::LeftButton);
    QCOMPARE(applied.count(), 1);
    QVERIFY(dialog.isVisible());
    QCOMPARE(AppSettings::fromStorage().scale(), 1.0);
}

void TestMainWindow::settingsDialogReadingOptions() {
    // Normalize first (entry state depends on test order).
    AppSettings baseline;
    baseline.setReading(ReadingDisplay::Kana);
    baseline.save();
    SettingsDialog dialog(nullptr);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    auto *combo = dialog.findChild<QComboBox *>("readingCombo");
    QVERIFY2(combo != nullptr, "reading combo missing");
    QCOMPARE(combo->count(), 3);
    QCOMPARE(dialog.selectedReading(), ReadingDisplay::Kana);
    auto *buttons = dialog.findChild<QDialogButtonBox *>();
    QVERIFY2(buttons != nullptr, "button box missing");
    auto *apply = buttons->button(QDialogButtonBox::Apply);
    QVERIFY2(apply != nullptr, "Apply button missing");
    QVERIFY(!apply->isEnabled());
    combo->setCurrentIndex(combo->findData(static_cast<int>(ReadingDisplay::Hepburn)));
    QCOMPARE(dialog.selectedReading(), ReadingDisplay::Hepburn);
    QVERIFY(apply->isEnabled());
    baseline.save();
}

void TestMainWindow::settingsDialogUpdatesOptOut() {
    // Updates page: automatic checks on by default; unchecking
    // dirties the dialog and reports off (persisted by MainWindow).
    AppSettings baseline;
    baseline.setCheckUpdates(true);
    baseline.save();
    SettingsDialog dialog(nullptr);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    auto *check = dialog.findChild<QCheckBox *>("checkUpdatesBox");
    QVERIFY2(check != nullptr, "updates checkbox missing");
    QVERIFY(check->isChecked());
    QVERIFY(dialog.selectedCheckUpdates());
    auto *buttons = dialog.findChild<QDialogButtonBox *>();
    QVERIFY2(buttons != nullptr, "button box missing");
    auto *apply = buttons->button(QDialogButtonBox::Apply);
    QVERIFY2(apply != nullptr, "Apply button missing");
    QVERIFY(!apply->isEnabled());
    check->setChecked(false);
    QVERIFY(!dialog.selectedCheckUpdates());
    QVERIFY(apply->isEnabled());
    baseline.save();
}
