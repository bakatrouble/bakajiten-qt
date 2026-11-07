#include "TestMainWindow.h"

#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QScrollArea>
#include <QSignalSpy>
#include <QSlider>
#include <QStackedWidget>
#include <QStyleFactory>
#include <QTimer>

#include "core/data/DataPaths.h"
#include "core/settings/Settings.h"
#include "ui/data/DictionaryManagerDialog.h"
#include "ui/data/DictionaryManagerRow.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/settings/SettingsDialog.h"
#include "util/Romaji.h"

#include <QProgressBar>
#include <QPushButton>
#include <memory>

void TestMainWindow::settingsDialogAppliesOnChange() {
    // Normalize all persisted values first (entry state depends on
    // test order).
    AppSettings baseline;
    baseline.setScale(1.0);
    baseline.setStyle(QString());
    baseline.save();
    AppSettings::shared().load();
    SettingsDialog dialog(nullptr);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    QSignalSpy changed(&dialog, &SettingsDialog::settingsChanged);
    // No icons combo: canvas paper follows the theme now.
    QVERIFY(dialog.findChild<QComboBox *>("iconsCombo") == nullptr);
    // The slider stages without reporting (scale commits on OK):
    // the restart notice arms, no signal fires.
    auto *slider = dialog.findChild<QSlider *>();
    QVERIFY2(slider != nullptr, "scale slider missing");
    auto *warning = dialog.findChild<QLabel *>("scaleWarning");
    QVERIFY2(warning != nullptr, "scale warning missing");
    QVERIFY(!warning->isVisibleTo(&dialog));
    slider->setValue(150);
    QCOMPARE(changed.count(), 0);
    QVERIFY(warning->isVisibleTo(&dialog));
    slider->setValue(100);
    QCOMPARE(changed.count(), 0);
    QVERIFY(!warning->isVisibleTo(&dialog));
    // Slider stops at 25% steps: in-between values snap.
    slider->setValue(137);
    QCOMPARE(slider->value(), 125);
    QCOMPARE(changed.count(), 0);
    slider->setValue(100);
    // Theme reports immediately.
    auto *theme = dialog.findChild<QComboBox *>("themeCombo");
    QVERIFY2(theme != nullptr, "theme combo missing");
    QVERIFY2(theme->count() >= 2, "no installed styles to select");
    theme->setCurrentIndex(1);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(dialog.currentSettings().style(), theme->currentData().toString());
    theme->setCurrentIndex(0);
    QCOMPARE(changed.count(), 2);
    // Mode reports immediately.
    auto *mode = dialog.findChild<QComboBox *>("modeCombo");
    QVERIFY2(mode != nullptr, "mode combo missing");
    QCOMPARE(mode->count(), 3);
    mode->setCurrentIndex(mode->findData(static_cast<int>(ThemeMode::Dark)));
    QCOMPARE(changed.count(), 3);
    QCOMPARE(dialog.currentSettings().mode(), ThemeMode::Dark);
    // Readings report immediately.
    auto *reading = dialog.findChild<QComboBox *>("readingCombo");
    QVERIFY2(reading != nullptr, "reading combo missing");
    reading->setCurrentIndex(reading->findData(static_cast<int>(ReadingDisplay::Hepburn)));
    QCOMPARE(changed.count(), 4);
    QCOMPARE(dialog.currentSettings().reading(), ReadingDisplay::Hepburn);
    // The updates checkbox reports immediately.
    auto *check = dialog.findChild<QCheckBox *>("checkUpdatesBox");
    QVERIFY2(check != nullptr, "updates checkbox missing");
    check->setChecked(!check->isChecked());
    QCOMPARE(changed.count(), 5);
    QCOMPARE(dialog.currentSettings().checkUpdates(), check->isChecked());
    baseline.save();
}

void TestMainWindow::settingsDialogReadingAppliesLive() {
    // Normalize first (entry state depends on test order).
    AppSettings baseline;
    baseline.setScale(1.0);
    baseline.setStyle(QString());
    baseline.setReading(ReadingDisplay::Kana);
    baseline.setCheckUpdates(true);
    baseline.save();
    AppSettings::shared().load();
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *action = window->findChild<QAction *>("actionSettings");
    QVERIFY2(action != nullptr, "settings action missing");
    // Drive the modal through the real MainWindow path: flip the
    // readings combo and read the effect back. Non-scale settings
    // apply on change, so the dialog stays open and nothing
    // recreates. Results land in locals (asserted after the modal
    // returns, so a failure can never leave the dialog open to
    // hang the suite).
    bool sawDialog = false;
    bool persisted = false;
    bool staysOpen = false;
    bool restored = false;
    QTimer::singleShot(500, [&] {
        auto *dialog = qobject_cast<SettingsDialog *>(QApplication::activeModalWidget());
        if (dialog == nullptr) {
            return;
        }
        sawDialog = true;
        auto *reading = dialog->findChild<QComboBox *>("readingCombo");
        const int hepburn =
            reading != nullptr ? reading->findData(static_cast<int>(ReadingDisplay::Hepburn)) : -1;
        const int kana =
            reading != nullptr ? reading->findData(static_cast<int>(ReadingDisplay::Kana)) : -1;
        if (reading != nullptr && hepburn >= 0 && kana >= 0) {
            reading->setCurrentIndex(hepburn);
            // Persisted through the MainWindow handler, dialog open.
            persisted = AppSettings::fromStorage().reading() == ReadingDisplay::Hepburn;
            staysOpen = dialog->isVisible() && !MainWindow::consumeRecreateRequested();
            // Restore the entry state the same way (keeps later
            // tests hermetic), then close.
            reading->setCurrentIndex(kana);
            restored = AppSettings::fromStorage().reading() == ReadingDisplay::Kana;
        }
        dialog->reject();
    });
    action->trigger();
    // Back from the modal: applied live, restored, no recreation.
    QVERIFY(sawDialog);
    QVERIFY(persisted);
    QVERIFY(staysOpen);
    QVERIFY(restored);
    QCOMPARE(AppSettings::fromStorage().reading(), ReadingDisplay::Kana);
}

void TestMainWindow::settingsDialogModeAppliesLive() {
    // Normalize first (entry state depends on test order). A
    // non-default scale proves mode switches never reset the
    // interface font (style re-creation drops it otherwise).
    AppSettings baseline;
    baseline.setScale(1.5);
    baseline.setStyle(QString());
    baseline.setMode(ThemeMode::System);
    baseline.save();
    AppSettings::shared().load();
    AppSettings::shared().apply();
    const double scaledFont = QApplication::font().pointSizeF();
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *action = window->findChild<QAction *>("actionSettings");
    QVERIFY2(action != nullptr, "settings action missing");
    // Drive the modal through the real MainWindow path: switch
    // the color mode and read the effect back. The palette swaps
    // live (no recreation), persists, and the dialog stays open.
    // Results land in locals (asserted after the modal returns, so
    // a failure can never leave the dialog open to hang the suite).
    bool sawDialog = false;
    bool persisted = false;
    bool dark = false;
    bool staysOpen = false;
    bool restored = false;
    bool fontKept = true;
    QTimer::singleShot(500, [&] {
        auto *dialog = qobject_cast<SettingsDialog *>(QApplication::activeModalWidget());
        if (dialog == nullptr) {
            return;
        }
        sawDialog = true;
        auto *mode = dialog->findChild<QComboBox *>("modeCombo");
        const int darkAt = mode != nullptr ? mode->findData(static_cast<int>(ThemeMode::Dark)) : -1;
        const int systemAt =
            mode != nullptr ? mode->findData(static_cast<int>(ThemeMode::System)) : -1;
        if (mode != nullptr && darkAt >= 0 && systemAt >= 0) {
            mode->setCurrentIndex(darkAt);
            // Persisted through the MainWindow handler, palette
            // live, dialog open, scale font untouched.
            persisted = AppSettings::fromStorage().mode() == ThemeMode::Dark;
            dark = QApplication::palette().color(QPalette::Window).lightness() < 128;
            fontKept = fontKept &&
                       qFuzzyCompare(QApplication::font().pointSizeF() + 1.0, scaledFont + 1.0);
            staysOpen = dialog->isVisible() && !MainWindow::consumeRecreateRequested();
            // Restore the entry state the same way (keeps later
            // tests hermetic), then close.
            mode->setCurrentIndex(systemAt);
            fontKept = fontKept &&
                       qFuzzyCompare(QApplication::font().pointSizeF() + 1.0, scaledFont + 1.0);
            restored = AppSettings::fromStorage().mode() == ThemeMode::System;
        }
        dialog->reject();
    });
    action->trigger();
    // Back from the modal: applied live, restored, no recreation.
    QVERIFY(sawDialog);
    QVERIFY(persisted);
    QVERIFY(dark);
    QVERIFY(fontKept);
    QVERIFY(staysOpen);
    QVERIFY(restored);
    QCOMPARE(AppSettings::fromStorage().mode(), ThemeMode::System);
    baseline.setScale(1.0);
    baseline.save();
    AppSettings::shared().load();
    AppSettings::shared().apply();
}

void TestMainWindow::settingsDialogPopupFollowsTheme() {
    // A runtime style switch must not leave combo popups measuring
    // with the old style: reopened popups space rows like combos
    // born under the new style (the dialog rebuilds its combos on
    // a style change).
    const QString current =
        QApplication::style() != nullptr ? QApplication::style()->objectName() : QString();
    QString target;
    for (const QString &name : QStyleFactory::keys()) {
        if (name.compare(current, Qt::CaseInsensitive) != 0) {
            target = name;
            break;
        }
    }
    QVERIFY2(!target.isEmpty(), "need a second installed style");
    AppSettings baseline;
    baseline.setScale(1.0);
    baseline.setStyle(QString());
    baseline.setMode(ThemeMode::System);
    baseline.save();
    AppSettings::shared().load();
    AppSettings::shared().apply();
    const auto rowHeights = [](QComboBox *combo) {
        combo->showPopup();
        QApplication::processEvents();
        QStringList heights;
        for (int r = 0; r < combo->count(); ++r) {
            heights << QString::number(
                combo->view()->visualRect(combo->model()->index(r, 0)).height());
        }
        combo->hidePopup();
        QApplication::processEvents();
        return heights.join(QStringLiteral(","));
    };
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *action = window->findChild<QAction *>("actionSettings");
    QVERIFY2(action != nullptr, "settings action missing");
    // Results land in locals (asserted after the modal returns, so
    // a failure can never leave the dialog open to hang the suite:
    // no asserts inside the modal handler, reject always runs).
    bool sawDialog = false;
    bool dialogVisible = false;
    int comboCount = 0;
    bool rebuilt = false;
    bool visible = false;
    bool rowsMatch = false;
    bool restored = false;
    QTimer::singleShot(500, [&] {
        auto *dialog = qobject_cast<SettingsDialog *>(QApplication::activeModalWidget());
        if (dialog == nullptr) {
            return;
        }
        sawDialog = true;
        dialogVisible = dialog->isVisible();
        auto *combo = dialog->findChild<QComboBox *>("themeCombo");
        const int targetAt = combo != nullptr ? combo->findData(target) : -1;
        const int homeAt = combo != nullptr ? combo->findData(QString()) : -1;
        if (combo == nullptr || targetAt < 0 || homeAt < 0) {
            dialog->reject();
            return;
        }
        QComboBox *before = combo;
        combo->setCurrentIndex(targetAt);
        // Rebuilt in place on the style switch: the old instance
        // lingers until deleteLater runs, so expect both — the
        // fresh one carries the name, the selection, and a place
        // in the layout.
        const auto combos = dialog->findChildren<QComboBox *>("themeCombo");
        comboCount = combos.size();
        auto *after = combos.isEmpty() ? nullptr : combos.last();
        rebuilt = combos.size() == 2 && after != nullptr && after != before &&
                  after->currentData().toString() == target;
        if (rebuilt) {
            visible = after->isVisibleTo(dialog);
            // Fresh combo born under the new style, same items:
            // reopened popup rows must agree with it.
            auto *fresh = new QComboBox(dialog);
            for (int r = 0; r < after->count(); ++r) {
                fresh->addItem(after->itemText(r), after->itemData(r));
            }
            rowsMatch = rowHeights(after) == rowHeights(fresh);
            // Restore the entry state the same way (keeps later
            // tests hermetic), then close.
            after->setCurrentIndex(homeAt);
            restored = AppSettings::fromStorage().style().isEmpty();
        }
        dialog->reject();
    });
    action->trigger();
    qDebug() << "popup-test sawDialog" << sawDialog << "dialogVisible" << dialogVisible
             << "comboCount" << comboCount;
    QVERIFY(sawDialog);
    QVERIFY(rebuilt);
    QVERIFY(visible);
    QVERIFY(rowsMatch);
    QVERIFY(restored);
}

void TestMainWindow::settingsDialogReportsScale() {
    // Normalize first (entry state depends on test order).
    AppSettings baseline;
    baseline.setScale(1.0);
    baseline.setStyle(QString());
    baseline.save();
    AppSettings::shared().load();
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
    // Untouched: current factor, no warning, OK disabled (OK is
    // only for a staged scale — everything else applies on change).
    QCOMPARE(dialog.currentSettings().scale(), 1.0);
    auto *warning = dialog.findChild<QLabel *>("scaleWarning");
    QVERIFY2(warning != nullptr, "scale warning missing");
    QVERIFY(!warning->isVisibleTo(&dialog));
    auto *buttons = dialog.findChild<QDialogButtonBox *>();
    QVERIFY2(buttons != nullptr, "button box missing");
    auto *ok = buttons->button(QDialogButtonBox::Ok);
    QVERIFY2(ok != nullptr, "OK button missing");
    QVERIFY(!ok->isEnabled());
    // Moved: new staged factor, warning shown, OK arms — but the
    // slider never reports (MainWindow persists nothing until OK).
    QSignalSpy changed(&dialog, &SettingsDialog::settingsChanged);
    auto *slider = dialog.findChild<QSlider *>();
    QVERIFY2(slider != nullptr, "scale slider missing");
    slider->setValue(150);
    QCOMPARE(dialog.currentSettings().scale(), 1.5);
    QVERIFY(warning->isVisibleTo(&dialog));
    QVERIFY(ok->isEnabled());
    QCOMPARE(changed.count(), 0);
    QCOMPARE(AppSettings::fromStorage().scale(), 1.0);
    // Restored: notice clears, OK disarms.
    slider->setValue(100);
    QVERIFY(!warning->isVisibleTo(&dialog));
    QVERIFY(!ok->isEnabled());
    baseline.save();
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
    QCOMPARE(dialog.currentSettings().reading(), ReadingDisplay::Kana);
    QSignalSpy changed(&dialog, &SettingsDialog::settingsChanged);
    combo->setCurrentIndex(combo->findData(static_cast<int>(ReadingDisplay::Hepburn)));
    QCOMPARE(dialog.currentSettings().reading(), ReadingDisplay::Hepburn);
    QCOMPARE(changed.count(), 1);
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
    QCOMPARE(dialog.currentSettings().checkUpdates(), true);
    QSignalSpy changed(&dialog, &SettingsDialog::settingsChanged);
    check->setChecked(false);
    QCOMPARE(dialog.currentSettings().checkUpdates(), false);
    QCOMPARE(changed.count(), 1);
    baseline.save();
}

void TestMainWindow::dataDialogBuildsFromForm() {
    // The Dictionaries dialog comes from the Designer form: static
    // chrome (info, notice, button box) plus a scrollable sectioned
    // list — Required and Optional headers, one row widget per
    // artifact with its own (initially hidden) progress bar, like
    // the Android manager.
    DictionaryManagerDialog dialog(nullptr);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    auto *info = dialog.findChild<QLabel *>("infoLabel");
    QVERIFY2(info != nullptr, "info label missing");
    QVERIFY(info->wordWrap());
    auto *scroll = dialog.findChild<QScrollArea *>("rowsScroll");
    QVERIFY2(scroll != nullptr, "rows scroller missing");
    auto *row = dialog.findChild<QHBoxLayout *>("buttonRow");
    QVERIFY2(row != nullptr, "button row missing");
    const QStringList names({QStringLiteral("requiredButton"), QStringLiteral("updateButton"),
                             QStringLiteral("checkButton"), QStringLiteral("clearButton"),
                             QStringLiteral("openDirButton"), QStringLiteral("closeButton")});
    QList<const QPushButton *> rowButtons;
    const QMap<QString, QString> labels(
        {{QStringLiteral("requiredButton"), QStringLiteral("Download required")},
         {QStringLiteral("updateButton"), QStringLiteral("Update all")},
         {QStringLiteral("checkButton"), QStringLiteral("Check for updates")},
         {QStringLiteral("clearButton"), QStringLiteral("Clear cached files")},
         {QStringLiteral("openDirButton"), QStringLiteral("Open data directory")},
         {QStringLiteral("closeButton"), QStringLiteral("Close")}});
    for (const QString &name : names) {
        auto *button = dialog.findChild<QPushButton *>(name);
        QVERIFY2(button != nullptr, qPrintable(name + QStringLiteral(" missing from row")));
        QVERIFY2(row->indexOf(button) >= 0, qPrintable(name + QStringLiteral(" not in row")));
        QCOMPARE(button->text(), labels.value(name));
        rowButtons << button;
    }
    const QPushButton *openDir = dialog.findChild<QPushButton *>(QStringLiteral("openDirButton"));
    QVERIFY2(openDir->isEnabled(), "Open data directory button disabled");
    // Live row layout: visible buttons never overlap.
    QList<const QPushButton *> visibleRowButtons;
    for (const QPushButton *button : rowButtons) {
        if (button->isVisibleTo(&dialog)) {
            visibleRowButtons << button;
        }
    }
    QVERIFY2(!visibleRowButtons.isEmpty(), "no visible row buttons");
    for (int i = 0; i < visibleRowButtons.size(); ++i) {
        for (int j = i + 1; j < visibleRowButtons.size(); ++j) {
            QVERIFY2(
                !visibleRowButtons[i]->geometry().intersects(visibleRowButtons[j]->geometry()),
                qPrintable(QStringLiteral("%1 overlaps %2")
                               .arg(visibleRowButtons[i]->text(), visibleRowButtons[j]->text())));
        }
    }
    // Two section headers plus one row widget per artifact file,
    // with the Japanese handwriting row at the end of Required
    // (dl.google.com pack, not a release artifact).
    const auto rows = dialog.findChildren<DictionaryManagerRow *>();
    QCOMPARE(rows.size(), DataPaths::allFiles().size() + 1);
    int actions = 0;
    int removes = 0;
    bool requiredHeader = false;
    bool optionalHeader = false;
    for (const QLabel *label : scroll->findChildren<QLabel *>()) {
        if (label->text() == QStringLiteral("Required")) {
            requiredHeader = true;
        } else if (label->text() == QStringLiteral("Optional")) {
            optionalHeader = true;
        }
    }
    QVERIFY2(requiredHeader, "Required header missing");
    QVERIFY2(optionalHeader, "Optional header missing");
    bool handwritingRow = false;
    for (const DictionaryManagerRow *row : rows) {
        const auto rowButtons = row->findChildren<QPushButton *>();
        QCOMPARE(rowButtons.size(), 2);
        for (const QPushButton *button : rowButtons) {
            if (button->text() == QStringLiteral("Remove")) {
                ++removes;
            } else {
                ++actions;
            }
        }
        const auto rowBars = row->findChildren<QProgressBar *>();
        QCOMPARE(rowBars.size(), 1);
        QVERIFY(!rowBars.front()->isVisibleTo(&dialog));
        if (row->dataFile() == QStringLiteral("handwriting")) {
            handwritingRow = true;
            QCOMPARE(row->fileName(), QStringLiteral("Japanese handwriting"));
        }
    }
    QVERIFY2(handwritingRow, "handwriting row missing");
    // Required placement: after the required dictionary rows,
    // before anything optional (children order is layout order).
    int handwritingAt = -1;
    int lastRequiredAt = -1;
    int firstOptionalAt = -1;
    for (int i = 0; i < rows.size(); ++i) {
        const QString file = rows.at(i)->dataFile();
        if (file == QStringLiteral("handwriting")) {
            handwritingAt = i;
        } else if (DataPaths::requiredFiles().contains(file)) {
            lastRequiredAt = i;
        } else if (firstOptionalAt < 0) {
            firstOptionalAt = i;
        }
    }
    QVERIFY(handwritingAt > lastRequiredAt);
    QVERIFY(firstOptionalAt < 0 || handwritingAt < firstOptionalAt);
    QCOMPARE(actions, DataPaths::allFiles().size() + 1);
    QCOMPARE(removes, DataPaths::allFiles().size() + 1);
    // Required artifacts never offer individual delete: their
    // Remove buttons stay disabled (opt-ins stay armed); a full
    // reset goes through the Clear cached files button.
    const auto required = DataPaths::requiredFiles();
    for (const DictionaryManagerRow *row : rows) {
        const QString file = row->dataFile();
        QVERIFY2(file == QStringLiteral("handwriting") || DataPaths::allFiles().contains(file),
                 "row without artifact");
        // Handwriting presence is the cached pack (a directory),
        // not a data-dir file: read it off the rendered status.
        bool present = QFileInfo::exists(DataPaths::filePath(file));
        if (file == QStringLiteral("handwriting")) {
            const auto *status = row->findChild<QLabel *>("rowStatus");
            QVERIFY2(status != nullptr, "handwriting status missing");
            present = status->text().startsWith(QStringLiteral("Downloaded"));
        }
        const auto rowButtons = row->findChildren<QPushButton *>();
        for (const QPushButton *button : rowButtons) {
            if (button->text() != QStringLiteral("Remove")) {
                continue;
            }
            if (file == QStringLiteral("handwriting")) {
                // Required model: no individual Remove at all —
                // like the required dictionaries below.
                QVERIFY(!button->isVisibleTo(&dialog));
                QVERIFY(!button->isEnabled());
                continue;
            }
            QCOMPARE(button->isEnabled(), present && !required.contains(file));
        }
    }
}

void TestMainWindow::menuBarFollowsForcedPalette() {
    // Breeze paints the menu bar from its own theme colors, ignoring
    // even an explicitly set widget palette: with Light forced the
    // bar must still come out light (regression: it stayed dark
    // with dark-on-dark text).
    if (!QStyleFactory::keys().contains(QStringLiteral("Breeze"), Qt::CaseInsensitive)) {
        QSKIP("Breeze style not installed");
    }
    AppSettings baseline;
    baseline.setScale(1.0);
    baseline.setStyle(QStringLiteral("Breeze"));
    baseline.setMode(ThemeMode::Light);
    baseline.save();
    AppSettings::shared().load();
    AppSettings::shared().apply();
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *bar = window->findChild<QMenuBar *>("menubar");
    QVERIFY2(bar != nullptr, "menu bar missing");
    const QColor rendered = bar->grab().toImage().pixelColor(bar->rect().center());
    // Restore the entry state first (later tests must not inherit
    // the forced style), then assert.
    AppSettings restore;
    restore.setScale(1.0);
    restore.setStyle(QString());
    restore.setMode(ThemeMode::System);
    restore.save();
    AppSettings::shared().load();
    AppSettings::shared().apply();
    QVERIFY2(rendered.lightnessF() > 0.7, qPrintable(rendered.name()));
}

void TestMainWindow::dictionariesMenuLivesUnderFile() {
    // Dictionaries lives under File (data management sits with
    // Settings), not Help. Also covers the dialog's Open data
    // directory button (presence only — clicking would open the
    // real file manager).
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *bar = window->findChild<QMenuBar *>("menubar");
    QVERIFY2(bar != nullptr, "menu bar missing");
    const auto menus = bar->findChildren<QMenu *>();
    const QMenu *fileMenu = nullptr;
    const QMenu *helpMenu = nullptr;
    for (const QMenu *menu : menus) {
        if (menu->title() == QStringLiteral("&File")) {
            fileMenu = menu;
        } else if (menu->title() == QStringLiteral("&Help")) {
            helpMenu = menu;
        }
    }
    QVERIFY2(fileMenu != nullptr, "File menu missing");
    QVERIFY2(helpMenu != nullptr, "Help menu missing");
    const auto inMenu = [](const QMenu *menu, const QString &text) {
        for (const QAction *action : menu->actions()) {
            if (action->text() == text) {
                return true;
            }
        }
        return false;
    };
    QVERIFY2(inMenu(fileMenu, QStringLiteral("&Dictionaries…")), "Dictionaries not under File");
    QVERIFY2(!inMenu(helpMenu, QStringLiteral("&Dictionaries…")), "Dictionaries still under Help");
}

void TestMainWindow::systemModeUsesSelectedStylePalette() {
    // System mode follows the selected style's own palette, not the
    // startup OS capture (regression: vista style under a
    // windows11-captured white painted white panels). Fusion is
    // always available, headless-safe.
    if (!QStyleFactory::keys().contains(QStringLiteral("Fusion"), Qt::CaseInsensitive)) {
        QSKIP("Fusion style not installed");
    }
    AppSettings styled;
    styled.setScale(1.0);
    styled.setStyle(QStringLiteral("Fusion"));
    styled.setMode(ThemeMode::System);
    styled.save();
    AppSettings::shared().load();
    AppSettings::shared().apply();
    std::unique_ptr<QStyle> fusion(QStyleFactory::create(QStringLiteral("Fusion")));
    QVERIFY2(fusion != nullptr, "fusion style missing");
    const QColor live = QApplication::palette().color(QPalette::Window);
    const QColor native = fusion->standardPalette().color(QPalette::Window);
    // Restore the entry state first, then assert.
    AppSettings restore;
    restore.setScale(1.0);
    restore.setStyle(QString());
    restore.setMode(ThemeMode::System);
    restore.save();
    AppSettings::shared().load();
    AppSettings::shared().apply();
    QCOMPARE(live.name(), native.name());
}
