#pragma once

#include <QMainWindow>

class QFileSystemWatcher;
class QTimer;

namespace Ui {
class MainWindow;
}

class AppSettings;
class RecognizerWidget;
class KanjiHover;
class UpdateChecker;

/// Application main window, laid out in `MainWindow.ui`: the
/// dictionary (`DictionaryWidget`, promoted central widget) is the main
/// area; the handwriting recognizer floats over it, hidden by
/// default. It opens under the search row button (panel top-left at
/// the button's bottom-left) from the button itself, and hides on
/// click-away, focus loss, or its own close button.
/// Dev builds also auto-reload both content forms when their .ui
/// files change on disk (same path as the F5 action).
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /// Recreate pending (scale changed): main() loops and builds a
    /// fresh window instead of exiting. Consumed per event-loop run.
    static bool consumeRecreateRequested();

protected:
    /// Click-away hides the panel, with the same exemptions as focus.
    bool eventFilter(QObject *watched, QEvent *event) override;
    /// Interface font changed: the menu bar's hint updates but its
    /// laid-out height does not — relayout keeps scaled text inside.
    void changeEvent(QEvent *event) override;

private slots:
    void toggleRecognizer(bool visible);
    void onRecognizerClosed();
    /// Modal settings (interface scale et al.). A changed scale
    /// persists and recreates the window in-process (fresh
    /// geometries, same as a restart), on OK and on Apply alike.
    void openSettings();
    /// Dictionaries dialog (Help menu): download + update the data
    /// dir artifacts. On first run (required files missing) it
    /// opens so the user can start the download — nothing fetches
    /// without a click.
    void openDictionaries();
    /// First-run check: required artifacts missing means the
    /// dialog opens with their download already running.
    void checkData();
    /// Dev entry point for the F5 action (no-op in release: both
    /// reloads return false immediately).
    void reloadDevUi();
    /// A watched .ui file changed: re-arm if an atomic-save editor
    /// replaced it, then debounce the reload.
    void onUiFileChanged(const QString &path);
    /// Focus left for good: hide unless it landed in the panel, on
    /// its open button, or in a menu.
    void onFocusChanged(QWidget *old, QWidget *now);
    /// Update check answered with a downloadable build.
    void onUpdateFound(const QString &version, const QString &notes, const QString &asset,
                       const QString &url, qint64 size);
    /// Update check answered with a version but no build here.
    void onUpdateNoArtifact(const QString &version, const QString &notes);
    /// Manual check states (automatic checks stay silent).
    void onUpdateUpToDate();
    void onUpdateFailed(const QString &reason);
    /// Finished download location: offer the restart.
    void onDownloadFinished(const QString &path);
    void onDownloadFailed(const QString &reason);

private:
    /// Single visibility writer: panel + open-button check state,
    /// no loops (the check change is blocked from re-firing).
    void setRecognizerVisible(bool visible);
    /// Persist changed appearance; returns true when the scale
    /// changed and the window is closing for recreation (theme +
    /// icons apply live). Unchanged is a no-op (false). Shared by
    /// the OK and Apply paths.
    bool applyAppearance(const AppSettings &selected);

private:
    /// Watch both content .ui files; changes funnel into reloadDevUi.
    /// Dev builds only (no-op otherwise).
    void watchDevUi();

    Ui::MainWindow *ui;
    RecognizerWidget *recognizer;
    /// Window-level kanji hover (shared card for the results
    /// table, the article, and the recognizer strip).
    KanjiHover *kanjiHover = nullptr;
    /// Update checks: shortly after startup, then hourly while the
    /// opt-out stays off. Manual checks run on demand instead.
    UpdateChecker *updateChecker = nullptr;
    QTimer *updateTimer = nullptr;
    /// Dev-only (null in release): .ui watcher + reload debounce.
    QFileSystemWatcher *uiWatcher = nullptr;
    QTimer *uiReloadTimer = nullptr;
};
