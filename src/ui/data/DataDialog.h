#pragma once

#include <QDialog>
#include <QMap>
#include <QString>

#include <optional>

#include "core/data/DataManifest.h"

class DataChecker;
class DataDownloader;
class QGridLayout;
class QLabel;
class QProgressBar;
class QPushButton;

/// Dictionaries dialog (Help menu): one row per artifact with its
/// local status and a Download/Update button, plus a single button
/// for the whole required set and a manual update check. Nothing
/// downloads without a click — not even on first run. Names/kokugo
/// are opt-in (never auto-fetched). The layout never moves: action
/// buttons exist from the start (greyed out until the manifest
/// lands) and progress has its own bar. Emits dataChanged when
/// files land so the views reload.
class DataDialog : public QDialog {
    Q_OBJECT
public:
    explicit DataDialog(QWidget *parent = nullptr);

signals:
    /// Files landed on disk: reload artifacts.
    void dataChanged();

private slots:
    void onCheck();
    void onChecked(const DataManifest &remote, const QByteArray &bytes);
    void onCheckFailed(const QString &reason);
    void onRowAction();
    void onRequiredAction();
    void onDownloadProgress(const QString &file, qint64 received, qint64 total);
    void onDownloadFinished();
    void onDownloadFailed(const QString &reason);

private:
    struct Row {
        QLabel *status = nullptr;
        QPushButton *action = nullptr;
    };

    /// Re-render every row from local disk (+ remote when known).
    /// Buttons stay put throughout: only text and enabled change.
    void renderRows();
    /// Required files that are not current (missing or outdated).
    QStringList pendingRequired() const;
    /// Start downloading these manifest files (progress resets).
    void downloadFiles(const QStringList &files);
    void setBusy(bool busy);
    void updateProgress();

    QLabel *infoLabel = nullptr;
    QGridLayout *rowsGrid = nullptr;
    QProgressBar *progressBar = nullptr;
    QPushButton *requiredButton = nullptr;
    QPushButton *checkButton = nullptr;
    QMap<QString, Row> rows;
    DataChecker *checker = nullptr;
    DataDownloader *downloader = nullptr;
    std::optional<DataManifest> remote;
    QByteArray remoteBytes;
    bool downloadsLanded = false;
    /// Required batch actionable (set by renderRows; drives the
    /// required button without reading its text back).
    bool requiredActionable = false;
    /// In-flight batch (progress denominator + numerator).
    QStringList activeFiles;
    QMap<QString, qint64> activeReceived;
    qint64 activeTotalBytes = 0;
};
