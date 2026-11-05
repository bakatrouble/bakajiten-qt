#include "ui/data/DataDownloader.h"

#include "recognizer.hpp"

#include "core/data/DataPaths.h"
#include "core/data/DataSource.h"
#include "core/data/Gzip.h"
#include "ui/data/DataLog.h"
#include "util/Archive.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

DataDownloader::DataDownloader(QObject *parent) : QObject(parent) {
    network = new QNetworkAccessManager(this);
}

void DataDownloader::download(const DataManifest &remoteIn, const QByteArray &manifestBytesIn,
                              const QStringList &files) {
    cancel();
    manifestJob = true;
    remote = remoteIn;
    manifestBytes = manifestBytesIn;
    queue = files;
    if (queue.isEmpty()) {
        emit finished();
        return;
    }
    DataPaths::ensureDirectory();
    pump();
}

void DataDownloader::downloadModel(const QString &key, const std::vector<ModelPacks::Pack> &packs,
                                   const QString &rootDir) {
    cancel();
    manifestJob = false;
    modelKey = key;
    modelPacks = packs;
    modelRoot = rootDir;
    modelIndex = 0;
    modelBase = 0;
    modelReply = nullptr;
    if (modelPacks.empty()) {
        emit finished();
        return;
    }
    QDir().mkpath(QDir(modelRoot).filePath(QStringLiteral(".zips")));
    QDir().mkpath(QDir(modelRoot).filePath(QStringLiteral("ja")));
    startModelOne();
}

void DataDownloader::cancel() {
    aborting = true;
    queue.clear();
    // Aborts land in onReplyFinished, which only cleans up while
    // `aborting` holds (never reports mid-teardown).
    for (const Transfer &t : active) {
        if (t.reply != nullptr) {
            t.reply->abort();
        }
    }
    for (const Transfer &t : active) {
        delete t.output;
    }
    active.clear();
    aborting = false;
    modelKey.clear();
    modelPacks.clear();
    modelRoot.clear();
    modelIndex = 0;
    modelBase = 0;
    modelReply = nullptr;
}

void DataDownloader::pump() {
    while (static_cast<int>(active.size()) < kMaxParallel && !queue.isEmpty()) {
        startOne(queue.takeFirst());
    }
    maybeFinish();
}

void DataDownloader::startOne(const QString &file) {
    const QString finalPath = DataPaths::filePath(file);
    const QString gzPath = finalPath + QStringLiteral(".gz");
    const QString partPath = gzPath + QStringLiteral(".part");
    QFile *output = new QFile(partPath, this);
    if (!output->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(dataLog) << "cannot write" << partPath;
        delete output;
        failAll(tr("Cannot write %1").arg(partPath));
        return;
    }
    qCDebug(dataLog) << "downloading" << file;
    QNetworkRequest request{QUrl(DataSource::fileUrl(file))};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("bakajiten-data"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = network->get(request);
    connect(reply, &QNetworkReply::downloadProgress, this, &DataDownloader::onProgress);
    connect(reply, &QNetworkReply::finished, this, &DataDownloader::onReplyFinished);
    active.push_back(Transfer{file, reply, output, 0});
}

void DataDownloader::onProgress(qint64 received, qint64 /*total*/) {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    for (Transfer &t : active) {
        if (t.reply == reply) {
            t.receivedBytes = received;
            if (t.output != nullptr && t.output->isOpen()) {
                // QNetworkReply buffers: drain what arrived since the
                // last signal straight to disk (progress doubles as
                // the pump).
                t.output->write(reply->readAll());
            }
            if (reply == modelReply) {
                emit progress(modelKey, modelBase + received, modelTotal());
                return;
            }
            // Transport is the `.gz`: compressed size is the honest total.
            const qint64 known = remote.files.value(t.file).gz;
            emit progress(t.file, received, known);
            return;
        }
    }
}

void DataDownloader::onReplyFinished() {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    auto it = active.begin();
    for (; it != active.end(); ++it) {
        if (it->reply == reply) {
            break;
        }
    }
    if (it == active.end()) {
        return;
    }
    Transfer t = *it;
    active.erase(it);
    const bool ok = reply != nullptr && reply->error() == QNetworkReply::NoError;
    const QString failure = reply != nullptr ? reply->errorString() : tr("Download failed");
    if (ok && t.output != nullptr) {
        t.output->write(reply->readAll());
    }
    if (reply != nullptr) {
        reply->deleteLater();
    }
    const QString partPath = t.output != nullptr ? t.output->fileName() : QString();
    if (t.output != nullptr) {
        t.output->close();
        delete t.output;
    }
    const bool wasModel = reply != nullptr && reply == modelReply;
    modelReply = nullptr;
    if (aborting) {
        QFile::remove(partPath);
        maybeFinish();
        return;
    }
    if (wasModel) {
        if (!ok) {
            QFile::remove(partPath);
            qCWarning(dataLog) << "model download failed:" << failure;
            failAll(failure);
            return;
        }
        finishModelOne(partPath);
        return;
    }
    if (!ok) {
        QFile::remove(partPath);
        qCWarning(dataLog) << "download failed for" << t.file << ":" << failure;
        failAll(failure);
        return;
    }
    QFile gz(partPath);
    if (!gz.open(QIODevice::ReadOnly)) {
        qCWarning(dataLog) << "cannot read" << partPath;
        QFile::remove(partPath);
        failAll(tr("Cannot read %1").arg(partPath));
        return;
    }
    const std::optional<QByteArray> plain = Gzip::unpack(gz.readAll());
    gz.close();
    QFile::remove(partPath);
    if (!plain.has_value()) {
        qCWarning(dataLog) << "could not unpack" << t.file;
        failAll(tr("Could not unpack %1").arg(t.file));
        return;
    }
    // Hash-before-rename: a truncated asset never lands in place.
    if (DataManifestReader::sha256Of(*plain) != remote.files.value(t.file).sha256) {
        qCWarning(dataLog) << "checksum mismatch for" << t.file;
        failAll(tr("Checksum mismatch for %1").arg(t.file));
        return;
    }
    const QString binPart = DataPaths::filePath(t.file) + QStringLiteral(".part");
    QFile out(binPart);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
        out.write(*plain) != plain->size()) {
        QFile::remove(binPart);
        failAll(tr("Cannot write %1").arg(binPart));
        return;
    }
    out.close();
    const QString finalPath = DataPaths::filePath(t.file);
    QFile::remove(finalPath);
    if (!QFile::rename(binPart, finalPath)) {
        failAll(tr("Cannot move %1 into place").arg(t.file));
        return;
    }
    emit fileFinished(t.file);
    pump();
}

/// First failure wins: drop the queue, abort the rest (their
/// landings only clean up), report once.
void DataDownloader::failAll(const QString &reason) {
    if (aborting) {
        return;
    }
    aborting = true;
    queue.clear();
    for (const Transfer &t : active) {
        if (t.reply != nullptr) {
            t.reply->abort();
        }
    }
    for (const Transfer &t : active) {
        if (t.output != nullptr) {
            const QString partPath = t.output->fileName();
            t.output->close();
            delete t.output;
            QFile::remove(partPath);
        }
        if (t.reply != nullptr) {
            t.reply->deleteLater();
        }
    }
    active.clear();
    modelKey.clear();
    modelPacks.clear();
    modelRoot.clear();
    modelIndex = 0;
    modelBase = 0;
    modelReply = nullptr;
    aborting = false;
    emit failed(reason);
}

void DataDownloader::maybeFinish() {
    if (!queue.isEmpty() || !active.isEmpty() || aborting) {
        return;
    }
    // Dict jobs refresh the on-disk manifest; model jobs have
    // none (their catalog is compiled in).
    if (manifestJob) {
        manifestJob = false;
        QFile manifest(DataPaths::manifestPath());
        if (manifest.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            manifest.write(manifestBytes);
        }
    }
    emit finished();
}

qint64 DataDownloader::modelTotal() const {
    qint64 total = 0;
    for (const ModelPacks::Pack &pack : modelPacks) {
        total += pack.zipSize;
    }
    return total;
}

/// sha1 of a file, lowercase hex (matches the catalog table).
QString modelSha1(const QString &path, bool &ok) {
    QFile file(path);
    ok = file.open(QIODevice::ReadOnly);
    if (!ok) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha1);
    if (!hash.addData(&file)) {
        ok = false;
        return {};
    }
    return QString::fromLatin1(hash.result().toHex());
}

void DataDownloader::startModelOne() {
    // Skip what is already good: installed artifacts, then
    // verified cached zips (re-unpacked, never re-downloaded).
    while (modelIndex < modelPacks.size()) {
        const ModelPacks::Pack &pack = modelPacks[modelIndex];
        const QString dest = ModelPacks::artifactPath(modelRoot, pack);
        if (QFileInfo(dest).size() == pack.artifactSize) {
            modelBase += pack.zipSize;
            ++modelIndex;
            continue;
        }
        const QString zipped = ModelPacks::zipPath(modelRoot, pack);
        bool ok = false;
        if (QFileInfo(zipped).size() == pack.zipSize &&
            modelSha1(zipped, ok) == QString::fromLatin1(pack.sha1Hex) && ok) {
            QFile cached(zipped);
            if (!cached.open(QIODevice::ReadOnly)) {
                failAll(tr("Cannot read %1").arg(zipped));
                return;
            }
            QString reason;
            if (!installModelPack(pack, cached.readAll(), reason)) {
                failAll(reason);
                return;
            }
            modelBase += pack.zipSize;
            ++modelIndex;
            continue;
        }
        break;
    }
    if (modelIndex >= modelPacks.size()) {
        // Every source landed: convert once into the runtime
        // bundle before reporting (a cache hit on the sources
        // still funnels through here).
        QString reason;
        if (!ensureConverted(reason)) {
            failAll(reason);
            return;
        }
        emit fileFinished(modelKey);
        maybeFinish();
        return;
    }
    const ModelPacks::Pack &pack = modelPacks[modelIndex];
    const QString partPath = ModelPacks::zipPath(modelRoot, pack) + QStringLiteral(".part");
    QFile *output = new QFile(partPath, this);
    if (!output->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(dataLog) << "cannot write" << partPath;
        delete output;
        failAll(tr("Cannot write %1").arg(partPath));
        return;
    }
    qCDebug(dataLog) << "downloading model pack" << pack.pack;
    QNetworkRequest request{QUrl(QString::fromLatin1(pack.url))};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("bakajiten-data"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = network->get(request);
    connect(reply, &QNetworkReply::downloadProgress, this, &DataDownloader::onProgress);
    connect(reply, &QNetworkReply::finished, this, &DataDownloader::onReplyFinished);
    modelReply = reply;
    active.push_back(Transfer{modelKey, reply, output, 0});
}

void DataDownloader::finishModelOne(const QString &zipPart) {
    // Read back through QFile (the stream already closed it).
    QFile part(zipPart);
    if (!part.open(QIODevice::ReadOnly)) {
        QFile::remove(zipPart);
        failAll(tr("Cannot read %1").arg(zipPart));
        return;
    }
    const QByteArray bytes = part.readAll();
    part.close();
    const ModelPacks::Pack &pack = modelPacks[modelIndex];
    QString reason;
    if (!installModelPack(pack, bytes, reason)) {
        QFile::remove(zipPart);
        failAll(reason);
        return;
    }
    // Keep the verified zip: later runs skip the download.
    QFile::remove(ModelPacks::zipPath(modelRoot, pack));
    if (!QFile::rename(zipPart, ModelPacks::zipPath(modelRoot, pack))) {
        QFile::remove(zipPart);
        failAll(tr("Cannot cache %1").arg(QString::fromLatin1(pack.pack)));
        return;
    }
    modelBase += pack.zipSize;
    ++modelIndex;
    startModelOne();
}

bool DataDownloader::installModelPack(const ModelPacks::Pack &pack, const QByteArray &zipBytes,
                                      QString &reason) {
    if (QCryptographicHash::hash(zipBytes, QCryptographicHash::Sha1).toHex() !=
        QByteArray(pack.sha1Hex)) {
        reason = tr("Checksum mismatch for %1").arg(QString::fromLatin1(pack.pack));
        return false;
    }
    QVector<Archive::Entry> entries;
    if (!Archive::readZip(zipBytes, entries, reason)) {
        return false;
    }
    const Archive::Entry *wanted = nullptr;
    const QString suffix = QString::fromLatin1(pack.memberSuffix);
    for (const Archive::Entry &entry : entries) {
        if (!entry.symlink && entry.path.endsWith(suffix)) {
            if (wanted != nullptr) {
                reason =
                    tr("More than one wanted member in %1").arg(QString::fromLatin1(pack.pack));
                return false;
            }
            wanted = &entry;
        }
    }
    if (wanted == nullptr) {
        reason = tr("No *%1 member in %2").arg(suffix, QString::fromLatin1(pack.pack));
        return false;
    }
    if (wanted->data.size() != pack.artifactSize) {
        reason = tr("Member size mismatch in %1").arg(QString::fromLatin1(pack.pack));
        return false;
    }
    const QString dest = ModelPacks::artifactPath(modelRoot, pack);
    const QString tmp = dest + QStringLiteral(".part");
    QFile out(tmp);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
        out.write(wanted->data) != wanted->data.size()) {
        QFile::remove(tmp);
        reason = tr("Cannot write %1").arg(dest);
        return false;
    }
    out.close();
    QFile::remove(dest);
    if (!QFile::rename(tmp, dest)) {
        reason = tr("Cannot move %1 into place").arg(dest);
        return false;
    }
    return true;
}

/// Write bytes atomically (`.part` + rename).
bool writeAtomic(const QString &dest, const std::vector<std::uint8_t> &bytes, QString &reason) {
    const QString tmp = dest + QStringLiteral(".part");
    QFile out(tmp);
    const QByteArray data(reinterpret_cast<const char *>(bytes.data()),
                          static_cast<qsizetype>(bytes.size()));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(data) != data.size()) {
        QFile::remove(tmp);
        reason = QObject::tr("Cannot write %1").arg(dest);
        return false;
    }
    out.close();
    QFile::remove(dest);
    if (!QFile::rename(tmp, dest)) {
        reason = QObject::tr("Cannot move %1 into place").arg(dest);
        return false;
    }
    return true;
}

bool DataDownloader::ensureConverted(QString &reason) {
    const QString net = ModelPacks::safetensorsPath(modelRoot);
    if (QFileInfo(net).size() == ModelPacks::safetensorsSize()) {
        return true;
    }
    // Sources are installed (every pack landed above): convert
    // once into the bundle the recognizer loads.
    const std::vector<ModelPacks::Pack> packs = ModelPacks::packs();
    QFile recospecFile(ModelPacks::artifactPath(modelRoot, packs[0]));
    QFile tfliteFile(ModelPacks::artifactPath(modelRoot, packs[1]));
    if (!recospecFile.open(QIODevice::ReadOnly) || !tfliteFile.open(QIODevice::ReadOnly)) {
        reason = tr("Handwriting sources are missing");
        return false;
    }
    const QByteArray recospecBytes = recospecFile.readAll();
    const QByteArray tfliteBytes = tfliteFile.readAll();
    const std::vector<std::uint8_t> recospec(recospecBytes.begin(), recospecBytes.end());
    const std::vector<std::uint8_t> tflite(tfliteBytes.begin(), tfliteBytes.end());
    recognizer::ConvertedBundle bundle;
    try {
        bundle = recognizer::convert_model(tflite, recospec);
    } catch (const recognizer::RecognizerError &e) {
        reason = tr("Could not convert the handwriting model: %1").arg(QString::fromUtf8(e.what()));
        return false;
    }
    if (!writeAtomic(net, bundle.safetensors, reason)) {
        return false;
    }
    // The engine decodes without the word-level language model, so
    // the obsolete pack (12 MB zip + 20 MB artifact) never
    // downloads again and existing copies go away.
    QFile::remove(QDir(modelRoot).filePath(QStringLiteral("ja/lm.fst")));
    QFile::remove(
        QDir(modelRoot).filePath(QStringLiteral(".zips/ja_20190322_compact_fst_zip.zip")));
    return QFileInfo(net).size() == ModelPacks::safetensorsSize();
}
