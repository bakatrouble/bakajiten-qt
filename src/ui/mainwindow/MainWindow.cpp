#include "ui/mainwindow/MainWindow.h"

#include "ui_MainWindow.h"

#include "dev/UiForm.h"
#include "widgets/tooltip/KanjiHover.h"

#include <QApplication>
#include <QFileSystemWatcher>
#include <QLabel>
#include <QMessageBox>
#include <QTimer>

#include "core/data/DataPaths.h"
#include "core/settings/MemorizedState.h"
#include "core/settings/Settings.h"
#include "dev/UiDev.h"
#include "ui/data/DataDialog.h"
#include "ui/dictionary/DictionaryWidget.h"
#include "ui/recognizer/RecognizerWidget.h"
#include "ui/settings/SettingsDialog.h"
#include "ui/update/UpdateChecker.h"
#include "ui/update/UpdateProgress.h"
#include "ui/update/UpdatePrompt.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDialog>
#include <QFile>
#include <QFileSystemWatcher>
#include <QKeySequence>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPoint>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include <Qt>
#include <QtGlobal>

namespace {
// In-process restart flag for scale changes (main() consumes it).
bool recreateRequested = false;
} // namespace

bool MainWindow::consumeRecreateRequested() {
    const bool pending = recreateRequested;
    recreateRequested = false;
    return pending;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), recognizer(nullptr) {
    ui->setupUi(this);

    // Floating panel parented to the .ui's central widget.
    recognizer = new RecognizerWidget(ui->centralwidget);

    // Floating panel: positioned on every open (top-left at the
    // open button's bottom-left, clamped into the central widget).
    // (Parent comes from the .ui's central widget.) It sizes itself:
    // persisted size if any, else the default — the form fills the
    // shell, but nothing sizes the shell itself (it is a free child,
    // not in any layout). Hidden by default: the search button
    // opens it.
    // Explicit hide: a never-shown child would implicitly show with
    // its parent, defeating hidden-by-default.
    recognizer->hide();

    connect(ui->actionQuit, &QAction::triggered, qApp, &QApplication::quit);
    connect(ui->actionSettings, &QAction::triggered, this, &MainWindow::openSettings);
    connect(ui->actionAbout, &QAction::triggered, this, [this] {
        // Manual box (not the static helper) so source links open
        // externally.
        QMessageBox box(QMessageBox::NoIcon, tr("About bakajiten"), QString(), QMessageBox::Ok,
                        this);
        box.setTextFormat(Qt::RichText);
        box.setText(
            tr("bakajiten %1 by <a href=\"https://github.com/bakatrouble/\">bakatrouble</a>"
               "<br>Japanese dictionary lookup with handwriting recognition.")
                .arg(QCoreApplication::applicationVersion()) +
            tr("<br><br>Dictionaries: <a href=\"http://www.edrdg.org/\">JMdict</a>, "
               "<a href=\"http://www.edrdg.org/\">JMnedict</a> and <a "
               "href=\"http://www.edrdg.org/\">KANJIDIC2</a> (Electronic Dictionary Research and "
               "Development Group, CC-BY-SA); kokugo entries from <a "
               "href=\"https://kaikki.org/jawiktionary/rawdata.html\">Japanese Wiktionary</a> "
               "(CC-BY-SA 4.0) and the <a href=\"https://bond-lab.github.io/wnja/\">Japanese "
               "WordNet</a> (CC-BY 4.0); JLPT levels by Jonathan Waller (<a "
               "href=\"http://www.tanos.co.uk/jlpt/\">Tanos</a>, CC-BY).") +
            tr("<br><br>Kanji: stroke data from <a "
               "href=\"https://kanjivg.tagaini.net/\">KanjiVG</a> (CC-BY-SA 3.0).") +
            tr("<br><br>Japanese font fallback (Windows): <a "
               "href=\"https://github.com/google/fonts\">Noto Sans JP</a> (SIL Open Font "
               "License 1.1, bundled).") +
            tr("<br><br>Inspiration: handwriting recognition, deinflection, relevance ranking — "
               "and the interface itself — follow <a "
               "href=\"https://github.com/z1dev/zkanji\">zkanji</a> by Sólyom Zoltán (see the "
               "<a "
               "href=\"https://zkanji.wordpress.com/2011/01/09/"
               "zkanji-under-the-hood-handwriting-recognition/\">under-the-hood "
               "writeup</a>)."));
        for (auto *label : box.findChildren<QLabel *>()) {
            label->setOpenExternalLinks(true);
        }
        box.exec();
    });
    connect(ui->actionAboutQt, &QAction::triggered, this, [this] { QMessageBox::aboutQt(this); });
    // Updates: manual check from Help; automatic shortly after
    // startup and hourly while the opt-out stays off. Unset
    // artifact builds (distro packages with their own update flow)
    // never phone home: hide the manual check instead of a dead
    // menu item (the checker itself already no-ops).
    if (QStringLiteral(BAKAJITEN_UPDATE_ARTIFACT).isEmpty()) {
        ui->actionCheckUpdates->setVisible(false);
    }
    updateChecker = new UpdateChecker(this);
    connect(updateChecker, &UpdateChecker::updateFound, this, &MainWindow::onUpdateFound);
    connect(updateChecker, &UpdateChecker::noArtifact, this, &MainWindow::onUpdateNoArtifact);
    connect(updateChecker, &UpdateChecker::upToDate, this, &MainWindow::onUpdateUpToDate);
    connect(updateChecker, &UpdateChecker::checkFailed, this, &MainWindow::onUpdateFailed);
    connect(ui->actionCheckUpdates, &QAction::triggered, this,
            [this] { updateChecker->check(true); });
    connect(ui->actionDictionaries, &QAction::triggered, this, [this] { openDictionaries(); });
    updateTimer = new QTimer(this);
    updateTimer->setInterval(3600 * 1000);
    connect(updateTimer, &QTimer::timeout, this, [this] {
        if (AppSettings::shared().checkUpdates()) {
            updateChecker->check(false);
        }
    });
    if (AppSettings::shared().checkUpdates()) {
        updateTimer->start();
        QTimer::singleShot(30000, this, [this] {
            if (AppSettings::shared().checkUpdates()) {
                updateChecker->check(false);
            }
        });
    }
    connect(recognizer, &RecognizerWidget::panelClosed, this, &MainWindow::onRecognizerClosed);
    connect(recognizer, &RecognizerWidget::candidateAccepted, ui->centralwidget,
            &DictionaryWidget::submit);
    connect(recognizer, &RecognizerWidget::candidateDoubleClicked, ui->centralwidget,
            &DictionaryWidget::openKanjiInfo);
    connect(ui->centralwidget, &DictionaryWidget::toggleRecognizer, this,
            &MainWindow::toggleRecognizer);
    // First-run data: required artifacts missing means the
    // dictionary + handwriting views sit empty, so offer the
    // download immediately (deferred past show, like the update
    // check below).
    QTimer::singleShot(0, this, &MainWindow::checkData);
    connect(qApp, &QApplication::focusChanged, this, &MainWindow::onFocusChanged);
    qApp->installEventFilter(this);
    // Window-level kanji hover (one shared card; recreated with
    // the forms on dev reload).
    kanjiHover = new KanjiHover(this, ui->centralwidget->kanjiData());

    // Dev only (discarded in release): rebuild both content forms
    // from the .ui files on disk. Recognizer/search state survives;
    // geometry does too (only the forms are swapped).
    if constexpr (kDevUiLoader) {
        QAction *reloadUi = ui->menuFile->addAction(tr("Reload UI (dev)"));
        reloadUi->setShortcut(QKeySequence(Qt::Key_F5));
        connect(reloadUi, &QAction::triggered, this, &MainWindow::reloadDevUi);
    }
    watchDevUi();
}

MainWindow::~MainWindow() {
    // Unhook app-level delivery first: teardown moves focus, which
    // would otherwise invoke slots on a half-destroyed object.
    qApp->removeEventFilter(this);
    disconnect(qApp, &QApplication::focusChanged, this, &MainWindow::onFocusChanged);
    delete ui;
}

void MainWindow::toggleRecognizer(bool visible) { setRecognizerVisible(visible); }

void MainWindow::openDictionaries() {
    // Modeless: downloads run in the background while the views
    // keep working; landed files reload both views at once.
    auto *dialog = new DataDialog(this);
    connect(dialog, &DataDialog::dataChanged, this, [this] {
        ui->centralwidget->refreshArtifacts();
        recognizer->reloadModel();
    });
    dialog->open();
}

void MainWindow::checkData() {
    for (const QString &file : DataPaths::requiredFiles()) {
        if (!QFile::exists(DataPaths::filePath(file))) {
            openDictionaries();
            return;
        }
    }
}

void MainWindow::openSettings() {
    // The dialog only reports; unchanged values close with no
    // effect. Changed appearance persists — a scale change closes
    // the window for in-process recreation (fresh geometries),
    // theme + icons apply live — on OK and on Apply alike.
    SettingsDialog dialog(this);
    connect(&dialog, &SettingsDialog::applyRequested, this, [this, &dialog] {
        AppSettings selected;
        selected.setScale(dialog.selectedScale());
        selected.setStyle(dialog.selectedStyle());
        selected.setIcons(dialog.selectedIcons());
        selected.setReading(dialog.selectedReading());
        selected.setCheckUpdates(dialog.selectedCheckUpdates());
        if (applyAppearance(selected)) {
            dialog.close();
        } else {
            // Persisted, staying open: the dialog's baseline is
            // stale until re-snapped.
            dialog.markApplied();
        }
    });
    if (dialog.exec() == QDialog::Accepted) {
        AppSettings selected;
        selected.setScale(dialog.selectedScale());
        selected.setStyle(dialog.selectedStyle());
        selected.setIcons(dialog.selectedIcons());
        selected.setReading(dialog.selectedReading());
        selected.setCheckUpdates(dialog.selectedCheckUpdates());
        (void)applyAppearance(selected);
    }
}

/// Persist changed appearance; true when the scale changed and the
/// window is closing for recreation. Unchanged is a no-op (false).
bool MainWindow::applyAppearance(const AppSettings &selected) {
    if (selected == AppSettings::shared()) {
        return false;
    }
    const bool scaleChanged =
        !qFuzzyCompare(selected.scale() + 1.0, AppSettings::shared().scale() + 1.0);
    const double oldScale = AppSettings::shared().scale();
    AppSettings::shared() = selected;
    AppSettings::shared().save();
    if (scaleChanged) {
        // Memorized widths are old-scale pixels: rescale once so
        // the fresh window keeps their share of the table.
        MemorizedState::rescaleWidths(selected.scale() / oldScale);
    }
    if (!scaleChanged) {
        // Theme + icons take effect immediately (style repolish,
        // canvas re-reads on repaint): persist, repaint, stay open.
        // Readings re-render the same way (no recreation needed).
        selected.applyStyle();
        recognizer->update();
        ui->centralwidget->refreshReadings();
        return false;
    }
    // Scale moves every geometry: apply the font now and recreate
    // the window in-process (main() loops). The settings warning
    // still says restart: a fresh window is one.
    AppSettings::shared().apply();
    recreateRequested = true;
    close();
    QApplication::quit();
    return true;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void MainWindow::onUpdateFound(const QString &version, const QString &notes, const QString &asset,
                               const QString &url, qint64 size) {
    UpdatePrompt prompt(version, notes, true, this);
    if (prompt.exec() != QDialog::Accepted) {
        if (prompt.skipVersion()) {
            MemorizedState::setSkippedUpdateVersion(version);
        }
        return;
    }
    if (prompt.skipVersion()) {
        MemorizedState::setSkippedUpdateVersion(version);
    }
    auto *progress = new UpdateProgress(version, url, asset, size, this);
    connect(progress, &UpdateProgress::downloadFinished, this, &MainWindow::onDownloadFinished);
    connect(progress, &UpdateProgress::downloadFailed, this, &MainWindow::onDownloadFailed);
    progress->start();
}

void MainWindow::onUpdateNoArtifact(const QString &version, const QString &notes) {
    UpdatePrompt prompt(version, notes, false, this);
    prompt.exec();
    if (prompt.skipVersion()) {
        MemorizedState::setSkippedUpdateVersion(version);
    }
}

void MainWindow::onUpdateUpToDate() {
    QMessageBox::information(
        this, tr("No updates"),
        tr("bakajiten %1 is the latest version.").arg(QCoreApplication::applicationVersion()));
}

void MainWindow::onUpdateFailed(const QString &reason) {
    QMessageBox::warning(this, tr("Update check failed"), reason);
}

void MainWindow::onDownloadFinished(const QString &path) {
    const auto answer = QMessageBox::question(
        this, tr("Update downloaded"), tr("The update is ready. Restart now to switch to it?"));
    if (answer != QMessageBox::Yes) {
        return;
    }
    // Hand off to the new binary, then quit (the old file stays put;
    // nothing is overwritten while running).
    QProcess::startDetached(path, {});
    QApplication::quit();
}

void MainWindow::onDownloadFailed(const QString &reason) {
    QMessageBox::warning(this, tr("Download failed"), reason);
}

void MainWindow::setRecognizerVisible(bool visible) {
    if (visible) {
        // Popover anchor: panel top-left at the button's bottom-left,
        // clamped so the panel stays inside the central widget (small
        // windows, long panels).
        const QPoint anchor = ui->centralwidget->recognizerButtonAnchor();
        const int x =
            qBound(0, anchor.x(), qMax(0, ui->centralwidget->width() - recognizer->width()));
        const int y =
            qBound(0, anchor.y(), qMax(0, ui->centralwidget->height() - recognizer->height()));
        recognizer->move(x, y);
    }
    recognizer->setVisible(visible);
    ui->centralwidget->setRecognizerVisible(visible);
    if (visible) {
        recognizer->raise();
        // Typing continues while drawing: the query field takes
        // focus (the auto-hide policy exempts it, like the panel).
        ui->centralwidget->focusSearchInput();
    }
}

void MainWindow::onRecognizerClosed() { setRecognizerVisible(false); }

void MainWindow::reloadDevUi() {
    ui->centralwidget->reloadUi();
    recognizer->reloadUi();
    // Forms rebuilt: the hover controller's scrollbar connections
    // point at dead widgets — rebuild it over the fresh ones.
    delete kanjiHover;
    kanjiHover = new KanjiHover(this, ui->centralwidget->kanjiData());
    // The rebuilt search form is added last, so it would cover the
    // sibling panel without this.
    recognizer->raise();
}

void MainWindow::watchDevUi() {
    if constexpr (!kDevUiLoader) {
        return;
    } else {
        uiWatcher = new QFileSystemWatcher(this);
        const QStringList forms = uiFormFiles();
        uiWatcher->addPaths(forms);
        qCInfo(uiLoaderLog) << "watching" << forms.size() << ".ui file(s)";
        uiReloadTimer = new QTimer(this);
        uiReloadTimer->setSingleShot(true);
        uiReloadTimer->setInterval(300);
        connect(uiWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::onUiFileChanged);
        connect(uiReloadTimer, &QTimer::timeout, this, &MainWindow::reloadDevUi);
    }
}

void MainWindow::onUiFileChanged(const QString &path) {
    // Atomic-save editors replace the file: re-add vanished paths so
    // the next save still fires.
    if (!uiWatcher->files().contains(path)) {
        uiWatcher->addPath(path);
    }
    // Coalesce the burst a single save produces.
    uiReloadTimer->start();
}
