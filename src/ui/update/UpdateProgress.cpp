#include "ui/update/UpdateProgress.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
UpdateProgress::UpdateProgress(const QString &version, const QString &url, const QString &fileName,
                               qint64 size, QWidget *parent)
    : QDialog(parent), version(version), url(url), fileName(fileName), expectedSize(size) {
    setWindowTitle(tr("Downloading bakajiten %1").arg(version));
    // Modeless on purpose: the download must never block the UI.
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose);
    auto *layout = new QVBoxLayout(this);
    bar = new QProgressBar(this);
    bar->setRange(0, size > 0 ? 1000 : 0);
    bar->setValue(0);
    layout->addWidget(bar);
    speedLabel = new QLabel(tr("Speed: —"), this);
    sizeLabel = new QLabel(tr("Downloaded: —"), this);
    etaLabel = new QLabel(tr("Remaining: —"), this);
    layout->addWidget(speedLabel);
    layout->addWidget(sizeLabel);
    layout->addWidget(etaLabel);
    cancelButton = new QPushButton(tr("Cancel"), this);
    connect(cancelButton, &QPushButton::clicked, this, &UpdateProgress::onCancel);
    layout->addWidget(cancelButton);
    network = new QNetworkAccessManager(this);
}

void UpdateProgress::start() {
    // Beside the running executable when writable, else the
    // platform download dir (system installs are not writable).
    QString dir = QCoreApplication::applicationDirPath();
    if (!QFileInfo(dir).isWritable()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    }
    finalPath = QDir(dir).filePath(fileName);
    partPath = finalPath + QStringLiteral(".part");
    output = new QFile(partPath, this);
    if (!output->open(QIODevice::WriteOnly)) {
        emit downloadFailed(tr("Cannot write %1").arg(partPath));
        deleteLater();
        return;
    }
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("bakajiten-update"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    reply = network->get(request);
    clock.start();
    connect(reply, &QNetworkReply::downloadProgress, this, &UpdateProgress::onProgress);
    connect(reply, &QNetworkReply::finished, this, &UpdateProgress::onReplyFinished);
    show();
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void UpdateProgress::onProgress(qint64 received, qint64 total) {
    receivedBytes = received;
    if (output != nullptr && output->isOpen()) {
        // QNetworkReply buffers: drain what arrived since the last
        // signal straight to disk (progress doubles as the pump).
        output->write(reply->readAll());
    }
    const qint64 known = total > 0 ? total : expectedSize;
    if (known > 0) {
        bar->setRange(0, 1000);
        bar->setValue(static_cast<int>(received * 1000 / known));
        sizeLabel->setText(tr("Downloaded: %1 / %2").arg(formatSize(received), formatSize(known)));
    } else {
        bar->setRange(0, 0);
        sizeLabel->setText(tr("Downloaded: %1").arg(formatSize(received)));
    }
    const double elapsed = qMax(static_cast<double>(clock.elapsed()) / 1000.0, 0.001);
    const double speed = static_cast<double>(received) / elapsed;
    speedLabel->setText(tr("Speed: %1").arg(formatSpeed(speed)));
    if (known > 0 && speed > 0.5) {
        etaLabel->setText(tr("Remaining: %1")
                              .arg(formatEta(static_cast<qint64>(
                                  static_cast<double>(known - received) / speed))));
    } else {
        etaLabel->setText(tr("Remaining: —"));
    }
}

void UpdateProgress::onReplyFinished() {
    const bool ok = reply != nullptr && reply->error() == QNetworkReply::NoError;
    const QString failure = reply != nullptr ? reply->errorString() : tr("Download failed");
    if (reply != nullptr) {
        if (ok) {
            output->write(reply->readAll());
        }
        reply->deleteLater();
        reply = nullptr;
    }
    if (output != nullptr) {
        output->close();
    }
    if (!ok) {
        QFile::remove(partPath);
        emit downloadFailed(failure);
        deleteLater();
        return;
    }
    if (expectedSize > 0 && receivedBytes != expectedSize) {
        // Truncated asset: never hand a short binary to restart.
        QFile::remove(partPath);
        emit downloadFailed(tr("Incomplete download (got %1 of %2)")
                                .arg(formatSize(receivedBytes), formatSize(expectedSize)));
        deleteLater();
        return;
    }
    QFile::remove(finalPath);
    if (!QFile::rename(partPath, finalPath)) {
        emit downloadFailed(tr("Cannot move %1 into place").arg(finalPath));
        deleteLater();
        return;
    }
#ifndef Q_OS_WIN
    QFile::setPermissions(finalPath, QFile::permissions(finalPath) | QFile::ExeOwner |
                                         QFile::ExeGroup | QFile::ExeOther);
#endif
    emit downloadFinished(finalPath);
    deleteLater();
}

void UpdateProgress::onCancel() {
    if (reply != nullptr) {
        reply->abort();
    }
    // The abort lands in onReplyFinished, which drops the partial.
}

QString UpdateProgress::formatSize(qint64 bytes) {
    if (bytes < 0) {
        return QStringLiteral("—");
    }
    constexpr qint64 kb = 1024;
    constexpr qint64 mb = 1024 * kb;
    if (bytes >= 10 * mb) {
        return tr("%1 MB").arg(static_cast<double>(bytes) / mb, 0, 'f', 1);
    }
    if (bytes >= mb) {
        return tr("%1 MB").arg(static_cast<double>(bytes) / mb, 0, 'f', 2);
    }
    if (bytes >= 10 * kb) {
        return tr("%1 KB").arg(static_cast<double>(bytes) / kb, 0, 'f', 0);
    }
    if (bytes >= kb) {
        return tr("%1 KB").arg(static_cast<double>(bytes) / kb, 0, 'f', 1);
    }
    return tr("%1 B").arg(bytes);
}

QString UpdateProgress::formatSpeed(double bytesPerSec) {
    if (!(bytesPerSec > 0.5)) {
        return QStringLiteral("—");
    }
    return tr("%1/s").arg(formatSize(static_cast<qint64>(bytesPerSec)));
}

QString UpdateProgress::formatEta(qint64 seconds) {
    if (seconds < 0 || seconds > 99LL * 3600) {
        return QStringLiteral("—");
    }
    return QStringLiteral("%1:%2")
        .arg(seconds / 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}
