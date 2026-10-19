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
    startNext();
}

void DataDownloader::cancel() {
    if (reply != nullptr) {
        // The abort lands in onReplyFinished, which drops the
        // partial and reports (never emits finished after this).
        reply->abort();
    }
    queue.clear();
}

void DataDownloader::startNext() {
    if (queue.isEmpty()) {
        QFile manifest(DataPaths::manifestPath());
        if (manifest.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            manifest.write(manifestBytes);
        }
        emit finished();
        return;
    }
    current = queue.takeFirst();
    const QString finalPath = DataPaths::filePath(current);
    const QString gzPath = finalPath + QStringLiteral(".gz");
    const QString partPath = gzPath + QStringLiteral(".part");
    output = new QFile(partPath, this);
    if (!output->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(dataLog) << "cannot write" << partPath;
        emit failed(tr("Cannot write %1").arg(partPath));
        delete output;
        output = nullptr;
        return;
    }
    qCDebug(dataLog) << "downloading" << current;
    QNetworkRequest request{QUrl(DataSource::fileUrl(current))};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("bakajiten-data"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    reply = network->get(request);
    receivedBytes = 0;
    connect(reply, &QNetworkReply::downloadProgress, this, &DataDownloader::onProgress);
    connect(reply, &QNetworkReply::finished, this, &DataDownloader::onReplyFinished);
}

void DataDownloader::onProgress(qint64 received, qint64 /*total*/) {
    receivedBytes = received;
    if (output != nullptr && output->isOpen() && reply != nullptr) {
        // QNetworkReply buffers: drain what arrived since the last
        // signal straight to disk (progress doubles as the pump).
        output->write(reply->readAll());
    }
    // Transport is the `.gz`: compressed size is the honest total.
    const qint64 known = remote.files.value(current).gz;
    emit progress(current, received, known);
}

void DataDownloader::onReplyFinished() {
    const bool ok = reply != nullptr && reply->error() == QNetworkReply::NoError;
    const QString failure = reply != nullptr ? reply->errorString() : tr("Download failed");
    if (reply != nullptr) {
        if (ok) {
            output->write(reply->readAll());
        }
        reply->deleteLater();
        reply = nullptr;
    }
    const QString partPath = output != nullptr ? output->fileName() : QString();
    if (output != nullptr) {
        output->close();
        delete output;
        output = nullptr;
    }
    if (!ok) {
        QFile::remove(partPath);
        qCWarning(dataLog) << "download failed for" << current << ":" << failure;
        emit failed(failure);
        return;
    }
    QFile gz(partPath);
    if (!gz.open(QIODevice::ReadOnly)) {
        qCWarning(dataLog) << "cannot read" << partPath;
        emit failed(tr("Cannot read %1").arg(partPath));
        return;
    }
    const std::optional<QByteArray> plain = Gzip::unpack(gz.readAll());
    gz.close();
    QFile::remove(partPath);
    if (!plain.has_value()) {
        qCWarning(dataLog) << "could not unpack" << current;
        emit failed(tr("Could not unpack %1").arg(current));
        return;
    }
    // Hash-before-rename: a truncated asset never lands in place.
    if (DataManifestReader::sha256Of(*plain) != remote.files.value(current).sha256) {
        qCWarning(dataLog) << "checksum mismatch for" << current;
        emit failed(tr("Checksum mismatch for %1").arg(current));
        return;
    }
    const QString binPart = DataPaths::filePath(current) + QStringLiteral(".part");
    QFile out(binPart);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
        out.write(*plain) != plain->size()) {
        QFile::remove(binPart);
        emit failed(tr("Cannot write %1").arg(binPart));
        return;
    }
    out.close();
    const QString finalPath = DataPaths::filePath(current);
    QFile::remove(finalPath);
    if (!QFile::rename(binPart, finalPath)) {
        emit failed(tr("Cannot move %1 into place").arg(current));
        return;
    }
    emit fileFinished(current);
    startNext();
}
