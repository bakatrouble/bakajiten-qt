#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "core/data/DataManifest.h"

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

/// Sequential raw-`.bin` download into the data dir: atomic `.part`
/// + rename per file, sha256-verified against the manifest, then
/// the manifest itself lands next to them. One file in flight;
/// `cancel` aborts and drops the partial.
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
    void startNext();

    QNetworkAccessManager *network = nullptr;
    QNetworkReply *reply = nullptr;
    QFile *output = nullptr;
    DataManifest remote;
    QByteArray manifestBytes;
    QStringList queue;
    QString current;
    qint64 receivedBytes = 0;
};
