#pragma once

#include <QFutureWatcher>
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
///
/// The network stays on the GUI thread (QNetworkAccessManager
/// affinity), but every blocking finish step — full-file gunzip,
/// hash passes, disk writes, model unzip + Rust conversion — runs
/// in a worker thread per transfer; only signals land back on the
/// GUI thread. A job epoch drops results that a later
/// download/cancel superseded.
///
/// Handwriting model packs ride the same signals under one key
/// (`downloadModel`): dl.google.com zips, sha1-verified, unzipped
/// member installed atomically, one pack at a time with cumulative
/// progress. A separate instance per job class keeps the manifest
/// write (dict jobs only) from ever touching model runs.
/// Blocking result of one dict-file finish (computed in a worker).
struct DictFinishResult {
    bool ok = false;
    QString reason;
    QString file;
    quint64 epoch = 0;
};

/// Inputs to one dict-file finish (one struct: no swappable
/// adjacent same-type parameters).
struct DictFinishInput {
    QString file;
    QString gzPart;
    QString sha;
    QString binPart;
    QString finalPath;
    quint64 epoch = 0;
};

/// Inputs to one model-pack install.
struct ModelInstallInput {
    QString rootDir;
    ModelPacks::Pack pack;
    QString zipSource;
    quint64 epoch = 0;
};

/// Blocking result of one model step — pack install or bundle
/// conversion (computed in a worker).
struct ModelStepResult {
    bool ok = false;
    QString reason;
    /// Finished-pack `.part` to promote to the zip cache (empty
    /// when the step read an already-cached zip).
    QString zipPart;
    quint64 epoch = 0;
};
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
    /// Dict-file worker landed: report or drop (superseded job),
    /// then keep pumping.
    void onDictFinish(QFutureWatcher<DictFinishResult> *watcher);
    /// Model-step worker landed: promote/advance or report, then
    /// continue the pack chain.
    void onModelStepDone(QFutureWatcher<ModelStepResult> *watcher);

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
    /// Hand the finished transfer's bytes to a worker thread
    /// (gunzip + hash + install there); the landing reports back.
    void launchDictFinish(const QString &file, const QString &gzPart);
    /// Install one finished pack in a worker thread (`promote`
    /// moves its `.part` into the zip cache on success).
    void launchModelInstall(const QString &zipSource, bool promote);
    /// Convert the installed sources in a worker thread.
    void launchModelConvert();
    /// Manifest lands + `finished`, once the queue and the in-flight
    /// set are both empty (manifest write for dict jobs only).
    void maybeFinish();
    /// Start the next model pack needing work (cached zips and
    /// installed artifacts skip the network; heavy steps run in
    /// workers and continue through onModelStepDone).
    void startModelOne();
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
    /// Job generation: download()/downloadModel()/cancel() bump it;
    /// worker landings from an older generation are dropped (their
    /// on-disk results are complete valid files, never partials).
    quint64 epoch = 0;
    /// Worker-thread finish steps currently running (their transfers
    /// already left `active`): maybeFinish waits for zero.
    int finishing = 0;
};
