#include "ui/data/DataDownloader.h"

#include "core/data/DataPaths.h"
#include "core/data/DataSource.h"
#include "core/data/Gzip.h"
#include "ui/data/DataLog.h"

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
    if (aborting) {
        QFile::remove(partPath);
        maybeFinish();
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
    aborting = false;
    emit failed(reason);
}

void DataDownloader::maybeFinish() {
    if (!queue.isEmpty() || !active.isEmpty() || aborting) {
        return;
    }
    QFile manifest(DataPaths::manifestPath());
    if (manifest.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        manifest.write(manifestBytes);
    }
    emit finished();
}
