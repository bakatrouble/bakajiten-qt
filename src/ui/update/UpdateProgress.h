#pragma once

#include <QDialog>
#include <QElapsedTimer>
#include <QString>

class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QProgressBar;
class QPushButton;
class QFile;

/// Non-blocking update download: progress bar plus speed,
/// downloaded/total, and ETA rows; Cancel aborts and drops the
/// partial file. Emits the finished path (caller prompts restart)
/// or a failure reason.
class UpdateProgress : public QDialog {
    Q_OBJECT
public:
    /// `size` is the release-asset byte count (progress falls back
    /// to unknown-total mode when it is 0).
    explicit UpdateProgress(const QString &version, const QString &url, const QString &fileName,
                            qint64 size, QWidget *parent = nullptr);

    /// Begin the download (atomic `.part` + rename, like fetch).
    void start();

    /// Human-sized byte count ("12.3 MB", "840 KB", "512 B").
    static QString formatSize(qint64 bytes);
    /// Human speed ("1.2 MB/s", "—" when stalled).
    static QString formatSpeed(double bytesPerSec);
    /// Human ETA ("mm:ss", "—" when unknown).
    static QString formatEta(qint64 seconds);

signals:
    void downloadFinished(const QString &path);
    void downloadFailed(const QString &reason);

private slots:
    void onProgress(qint64 received, qint64 total);
    void onReplyFinished();
    void onCancel();

private:
    QString version;
    QString url;
    QString fileName;
    qint64 expectedSize = 0;
    QNetworkAccessManager *network = nullptr;
    QNetworkReply *reply = nullptr;
    QFile *output = nullptr;
    QString partPath;
    QString finalPath;
    QElapsedTimer clock;
    qint64 receivedBytes = 0;
    QProgressBar *bar = nullptr;
    QLabel *speedLabel = nullptr;
    QLabel *sizeLabel = nullptr;
    QLabel *etaLabel = nullptr;
    QPushButton *cancelButton = nullptr;
};
