#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <QList>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/data/DataManifest.h"
#include "core/recognizer/ModelPack.h"

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

/// Capped-parallel `.bin` download into the data dir (4 in flight):
/// atomic `.part` + rename per file, sha256-verified against the
/// manifest, then the manifest itself lands next to them. Progress
/// reports per file (the dialog aggregates); `fileFinished` lands
/// per install. First failure cancels the rest and reports once.
/// `cancel` aborts silently and drops the partials.
///
/// Handwriting model packs ride the same signals under one key
/// (`downloadModel`): dl.google.com zips, sha1-verified, unzipped
/// member installed atomically, one pack at a time with cumulative
/// progress. A separate instance per job class keeps the manifest
/// write (dict jobs only) from ever touching model runs.
class DataDownloader : public QObject {
    Q_OBJECT
public:
    explicit DataDownloader(QObject *parent = nullptr);

    /// Download these manifest files (skips none — the caller
    /// decides the set). Empty sets finish immediately.
    void download(const DataManifest &remote, const QByteArray &manifestBytes,
                  const QStringList &files);
    /// Download a model pack set (verified zips cached under
    /// `<root>/.zips`, installed artifacts under `<root>/ja`).
    /// Cache hits skip the network.
    void downloadModel(const QString &key, const std::vector<ModelPacks::Pack> &packs,
                       const QString &rootDir);
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
    /// set are both empty (manifest write for dict jobs only).
    void maybeFinish();
    /// Start the next model pack needing work (cached zips and
    /// installed artifacts skip the network).
    void startModelOne();
    /// Verify + unpack + install the finished pack, then advance.
    void finishModelOne(const QString &zipPart);
    /// Convert the installed sources into the runtime bundle
    /// (`network.safetensors`) once every pack has landed; drops
    /// the obsolete word-LM artifacts. Pure work, no network.
    bool ensureConverted(QString &reason);
    /// Install one pack from zip bytes (sha1, single-member pick,
    /// size check, atomic rename). Pure work, no network.
    bool installModelPack(const ModelPacks::Pack &pack, const QByteArray &zipBytes,
                          QString &reason);
    /// Sum of the model pack zips (the honest download total).
    qint64 modelTotal() const;

    static constexpr int kMaxParallel = 4;

    QNetworkAccessManager *network = nullptr;
    DataManifest remote;
    QByteArray manifestBytes;
    QStringList queue;
    QList<Transfer> active;
    bool aborting = false;
    /// Model-pack job state (empty unless `downloadModel` ran).
    QString modelKey;
    std::vector<ModelPacks::Pack> modelPacks;
    QString modelRoot;
    std::size_t modelIndex = 0;
    qint64 modelBase = 0;
    QNetworkReply *modelReply = nullptr;
    /// Dict jobs write the manifest on finish; model jobs never do.
    bool manifestJob = false;
};
