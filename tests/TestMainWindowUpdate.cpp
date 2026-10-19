#include "TestMainWindow.h"

#include <QtTest>

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

#include "core/settings/MemorizedState.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/update/UpdateChecker.h"
#include "ui/update/UpdateProgress.h"
#include "ui/update/UpdatePrompt.h"

#include <memory>

void TestMainWindow::updaterVersionCompare() {
    // Tag/current comparison (`v` tolerated, garbage never newer).
    QVERIFY(UpdateChecker::isNewer(QStringLiteral("0.1.0"), QStringLiteral("v0.2.0")));
    QVERIFY(UpdateChecker::isNewer(QStringLiteral("0.1.0"), QStringLiteral("0.2.0")));
    QVERIFY(!UpdateChecker::isNewer(QStringLiteral("0.2.0"), QStringLiteral("v0.2.0")));
    QVERIFY(!UpdateChecker::isNewer(QStringLiteral("0.2.0"), QStringLiteral("v0.1.0")));
    QVERIFY(!UpdateChecker::isNewer(QStringLiteral(""), QStringLiteral("v0.2.0")));
    QVERIFY(!UpdateChecker::isNewer(QStringLiteral("0.1.0"), QStringLiteral("nightly")));
    QVERIFY(UpdateChecker::isNewer(QStringLiteral("0.1.0"), QStringLiteral("0.10.0")));
}

void TestMainWindow::updaterAssetMatch() {
    // Artifact kind picks the release asset by suffix.
    const QStringList names = {QStringLiteral("Bakajiten-x86_64.AppImage"),
                               QStringLiteral("bakajiten-win64-v0.1.0.zip"),
                               QStringLiteral("bakajiten-linux64-static.tar.gz"),
                               QStringLiteral("SHASUMS.txt")};
    QCOMPARE(UpdateChecker::matchedAsset(names, QStringLiteral("appimage")),
             QStringLiteral("Bakajiten-x86_64.AppImage"));
    QCOMPARE(UpdateChecker::matchedAsset({QStringLiteral("bakajiten.exe")},
                                        QStringLiteral("win64")),
             QStringLiteral("bakajiten.exe"));
    QCOMPARE(UpdateChecker::matchedAsset(names, QStringLiteral("linux-static")),
             QStringLiteral("bakajiten-linux64-static.tar.gz"));
    const QStringList macNames = {QStringLiteral("bakajiten-macos-arm64.zip"),
                                  QStringLiteral("bakajiten-macos-x64.zip")};
    QCOMPARE(UpdateChecker::matchedAsset(macNames, QStringLiteral("macos-arm64")),
             QStringLiteral("bakajiten-macos-arm64.zip"));
    QCOMPARE(UpdateChecker::matchedAsset(macNames, QStringLiteral("macos-x64")),
             QStringLiteral("bakajiten-macos-x64.zip"));
    QVERIFY(
        UpdateChecker::matchedAsset({QStringLiteral("bakajiten-win64-v0.1.0.zip")},
                                    QStringLiteral("appimage"))
            .isEmpty());
    QVERIFY(UpdateChecker::matchedAsset(names, QStringLiteral("")).isEmpty());
    QVERIFY(UpdateChecker::matchedAsset({}, QStringLiteral("appimage")).isEmpty());
}

void TestMainWindow::updaterPromptSkip() {
    // Artifact mode offers Download/Later; notice mode is Close
    // only; both carry the per-version opt-out, off by default.
    UpdatePrompt download(QStringLiteral("0.2.0"), QStringLiteral("notes"), true);
    QVERIFY(!download.skipVersion());
    auto *check = download.findChild<QCheckBox *>();
    QVERIFY2(check != nullptr, "skip checkbox missing");
    check->setChecked(true);
    QVERIFY(download.skipVersion());
    auto *buttons = download.findChild<QDialogButtonBox *>();
    QVERIFY2(buttons != nullptr, "button box missing");
    QVERIFY(buttons->button(QDialogButtonBox::Ok) == nullptr);
    bool hasDownload = false;
    for (auto *button : buttons->buttons()) {
        hasDownload = hasDownload || button->text() == QStringLiteral("Download");
    }
    QVERIFY(hasDownload);
    UpdatePrompt notice(QStringLiteral("0.2.0"), QStringLiteral("notes"), false);
    QVERIFY(!notice.skipVersion());
}

void TestMainWindow::updaterProgressFormat() {
    // Human rows for the progress dialog (boundaries included).
    QCOMPARE(UpdateProgress::formatSize(0), QStringLiteral("0 B"));
    QCOMPARE(UpdateProgress::formatSize(512), QStringLiteral("512 B"));
    QCOMPARE(UpdateProgress::formatSize(1024), QStringLiteral("1.0 KB"));
    QCOMPARE(UpdateProgress::formatSize(15360), QStringLiteral("15 KB"));
    QCOMPARE(UpdateProgress::formatSize(5 * 1024 * 1024), QStringLiteral("5.00 MB"));
    QCOMPARE(UpdateProgress::formatSize(48 * 1024 * 1024), QStringLiteral("48.0 MB"));
    QCOMPARE(UpdateProgress::formatSpeed(0.0), QStringLiteral("—"));
    QVERIFY(UpdateProgress::formatSpeed(1500.0).contains(QStringLiteral("/s")));
    QCOMPARE(UpdateProgress::formatEta(0), QStringLiteral("00:00"));
    QCOMPARE(UpdateProgress::formatEta(90), QStringLiteral("01:30"));
    QCOMPARE(UpdateProgress::formatEta(-1), QStringLiteral("—"));
    // Dialog builds modeless with a bar and a cancel path.
    UpdateProgress progress(QStringLiteral("0.2.0"), QStringLiteral("https://example.invalid/x"),
                            QStringLiteral("x.bin"), 100, nullptr);
    QVERIFY(!progress.isModal());
    QVERIFY2(progress.findChild<QProgressBar *>() != nullptr, "bar missing");
}

void TestMainWindow::updaterSkipPersists() {
    // The per-version opt-out round-trips through state.yaml;
    // suite state is shared, so it resets on the way out.
    MemorizedState::setSkippedUpdateVersion(QStringLiteral("0.2.0"));
    QCOMPARE(MemorizedState::skippedUpdateVersion(), QStringLiteral("0.2.0"));
    MemorizedState::setSkippedUpdateVersion(QString());
    QVERIFY(MemorizedState::skippedUpdateVersion().isEmpty());
    // Help menu carries the manual check.
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    // App icon (set on QApplication in main.cpp): the embedded
    // resource loads and renders.
    const QIcon appIcon(QStringLiteral(":/icons/app.svg"));
    QVERIFY2(!appIcon.isNull(), "app icon missing");
    QVERIFY2(!appIcon.pixmap(64, 64).isNull(), "app icon never renders");
    QVERIFY2(window->findChild<QAction *>("actionCheckUpdates") != nullptr,
             "check-updates action missing");
}
