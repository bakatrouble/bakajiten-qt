#pragma once

#include <QDialog>
#include <QMap>
#include <QString>

#include <optional>

#include "core/data/DataManifest.h"

namespace Ui {
class DictionaryManagerDialog;
}

class DataChecker;
class DataDownloader;
class DictionaryManagerRow;
class QLabel;
class QPushButton;
class QVBoxLayout;

/// Dictionaries dialog (Help menu): a scrollable list like the
/// Android manager — Required then Optional sections, one row
/// widget per artifact with its status (saved/latest versions +
/// sizes), Download/Update and Remove buttons, and its own
/// progress bar while it downloads. A single row of buttons below
/// runs the whole required set, all stale files, a manual update
/// check, or a full cache clear. Nothing downloads without a
/// click — not even on first run. Names/kokugo are opt-in (never
/// auto-fetched). Removes ask first (required files have no
/// Remove button at all). State lives in the DataStore singleton
/// (this view only renders its snapshots); emits dataChanged
/// when files land so the views reload.
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

private:
    /// Re-render every row from the DataStore snapshot (+ local
    /// disk); runs on every store emission. Buttons stay put:
    /// only text, visibility, and enabled change.
    void renderRows();
    /// Required files that are not current (missing or outdated).
    QStringList pendingRequired() const;
    /// Present-but-stale files (required or opt-in) for Update all.
    QStringList outdatedAll() const;
    /// Start downloading these manifest files.
    void downloadFiles(const QStringList &files);

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
};
