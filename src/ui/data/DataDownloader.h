#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <QList>

#include "core/data/DataManifest.h"

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

/// Capped-parallel `.bin` download into the data dir (4 in flight):
/// atomic `.part` + rename per file, sha256-verified against the
/// manifest, then the manifest itself lands next to them. Progress
/// reports per file (the dialog aggregates); `fileFinished` lands
/// per install. First failure cancels the rest and reports once.
/// `cancel` aborts silently and drops the partials.
class DataDownloader : public QObject {
    Q_OBJECT
public:
    explicit DataDownloader(QObject *parent = nullptr);

    /// Download these manifest files (skips none — the caller
    /// decides the set). Empty sets finish immediately.
    void download(const DataManifest &remote, const QByteArray &manifestBytes,
                  const QStringList &files);
    void cancel();

signals:
    void progress(const QString &file, qint64 received, qint64 total);
    void fileFinished(const QString &file);
    void finished();
    void failed(const QString &reason);

private slots:
    void onProgress(qint64 received, qint64 total);
    void onReplyFinished();

private:
    struct Transfer {
        QString file;
        QNetworkReply *reply = nullptr;
        QFile *output = nullptr;
        qint64 receivedBytes = 0;
    };

    /// Fill free slots from the queue (cap `kMaxParallel`).
    void pump();
    void startOne(const QString &file);
    /// First failure wins: drop the queue, abort the rest (their
    /// landings only clean up), report once.
    void failAll(const QString &reason);
    /// Manifest lands + `finished`, once the queue and the in-flight
    /// set are both empty.
    void maybeFinish();

    static constexpr int kMaxParallel = 4;

    QNetworkAccessManager *network = nullptr;
    DataManifest remote;
    QByteArray manifestBytes;
    QStringList queue;
    QList<Transfer> active;
    bool aborting = false;
};
