#pragma once

#include <QDialog>
#include <QFutureWatcher>
#include <QMap>
#include <QString>

#include <optional>

#include "core/data/DataManifest.h"
#include "core/state/DataStore.h"

namespace Ui {
class DictionaryManagerDialog;
}

class DataChecker;
class DataDownloader;
class DictionaryManagerRow;
class QLabel;
class QPushButton;
class QVBoxLayout;

/// One hashing pass over the release artifacts (worker thread):
/// per-file freshness against a remote manifest.
struct FileStatusResult {
    quint64 epoch = 0;
    /// Cache key the pass was computed for (remote manifest +
    /// on-disk stats); landings for a stale key are dropped.
    QString key;
    QMap<QString, DataFileStatus> statuses;
};

/// Dictionaries dialog (Help menu): a scrollable list like the
/// Android manager — Required then Optional sections, one row
/// widget per artifact with its status (saved/latest versions +
/// sizes), Download/Update and Remove buttons, and its own
/// progress bar while it downloads. The Required section ends
/// with the Japanese handwriting row (dl.google.com pack, not a
/// release artifact): same status/action/progress widgets, no
/// Remove button, bundled into Download required. A single row of buttons below runs the whole
/// required set, all stale files, a manual update check, or a full cache clear (which also drops
/// the handwriting pack). Nothing downloads without a click — not even on first run. Names/kokugo
/// are opt-in (never auto-fetched). Removes ask first (required files have no Remove button at
/// all). State lives in the DataStore singleton (this view only renders its snapshots); emits
/// dataChanged when files land so the views reload.
class DictionaryManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit DictionaryManagerDialog(QWidget *parent = nullptr);

signals:
    /// Files landed on disk: reload artifacts.
    void dataChanged();

private slots:
    void onCheck();
    void onChecked(const DataManifest &remote, const QByteArray &bytes);
    void onCheckFailed(const QString &reason);
    void onRowAction(const QString &file);
    void onRemoveAction(const QString &file);
    void onRequiredAction();
    void onUpdateAllAction();
    void onClearAllAction();
    void onDownloadProgress(const QString &file, qint64 received, qint64 total);
    void onDownloadFinished();
    void onDownloadFailed(const QString &reason);
    void onModelFileFinished(const QString &file);
    /// Hashing-pass worker landed: adopt its statuses (unless
    /// superseded) and repaint.
    void onStatusesDone(QFutureWatcher<FileStatusResult> *watcher);
    /// Handwriting pack download (same downloader as the release
    /// artifacts, separate instance so the manifest write stays
    /// with dict jobs; the row progress bar runs off the same
    /// snapshot flow).
    void onHandwritingDownload();

private:
    /// Re-render every row from the DataStore snapshot (+ local
    /// disk); runs on every store emission. File freshness comes
    /// from a cached hashing pass (see below) — never hashed
    /// inline, so progress ticks and dialog opens never stall.
    /// Buttons stay put: only text, visibility, and enabled change.
    void renderRows();
    /// Required files that are not current (missing or outdated),
    /// read off the cached hashing pass (empty until the first
    /// pass lands; the download buttons stay disabled meanwhile).
    QStringList pendingRequired() const;
    /// Present-but-stale files (required or opt-in) for Update all,
    /// same cache rules as pendingRequired().
    QStringList outdatedAll() const;
    /// Start downloading these manifest files.
    void downloadFiles(const QStringList &files);
    /// Render the Handwriting section row (dl.google.com pack,
    /// outside the manifest flow above).
    void renderHandwritingRow();
    /// True while the snapshot tracks an in-flight transfer for file.
    bool snapshotBusy(const QString &file) const;
    /// Cache key for the hashing pass: remote manifest bytes plus
    /// per-file on-disk stats (cheap). A pass runs only when the
    /// key moved (new remote manifest, landed/removed files).
    QString statusKeyFor(const DataStore::Snapshot &snapshot) const;
    /// Launch a hashing pass when the key moved (worker thread;
    /// landing repaints).
    void refreshStatuses(const DataStore::Snapshot &snapshot, const QString &key);
    /// Launch a hashing pass when the key moved (worker thread;
    /// landing repaints).
    void refreshStatuses(const DataStore::Snapshot &snapshot);
    /// Last hashing pass (worker thread) and the key it computed.
    /// Statuses paint rows; actions stay disabled until the first
    /// pass lands.
    QMap<QString, DataFileStatus> fileStatuses;
    bool statusesReady = false;
    QString statusKey;
    quint64 statusEpoch = 0;

    Ui::DictionaryManagerDialog *ui = nullptr;
    QWidget *devForm = nullptr;
    QLabel *infoLabel = nullptr;
    QVBoxLayout *rowsLayout = nullptr;
    QPushButton *requiredButton = nullptr;
    QPushButton *updateButton = nullptr;
    QPushButton *checkButton = nullptr;
    QPushButton *clearButton = nullptr;
    QMap<QString, DictionaryManagerRow *> rows;
    DataChecker *checker = nullptr;
    DataDownloader *downloader = nullptr;
    /// Set when a transfer lands (control flow for the dataChanged
    /// emit — not shared state, so it stays local).
    bool downloadsLanded = false;
    /// Handwriting pack row (dl.google.com, not a release
    /// artifact) plus its download thread.
    DictionaryManagerRow *handwritingRow = nullptr;
    DataDownloader *modelDownloader = nullptr;
    bool handwritingLanded = false;
};
