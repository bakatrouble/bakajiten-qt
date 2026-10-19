#include "ui/data/DataChecker.h"

#include "core/data/DataSource.h"
#include "ui/data/DataLog.h"

#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

Q_LOGGING_CATEGORY(dataLog, "bakajiten.data");

DataChecker::DataChecker(QObject *parent) : QObject(parent) {
    network = new QNetworkAccessManager(this);
}

void DataChecker::check() {
    if (busy) {
        return;
    }
    busy = true;
    qCDebug(dataLog) << "fetching manifest from" << DataSource::manifestUrl();
    QNetworkRequest request{QUrl(DataSource::manifestUrl())};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("bakajiten-data"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    connect(network->get(request), &QNetworkReply::finished, this, &DataChecker::onReplyFinished);
}

void DataChecker::onReplyFinished() {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply == nullptr) {
        busy = false;
        return;
    }
    reply->deleteLater();
    busy = false;
    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(dataLog) << "manifest fetch failed:" << reply->errorString();
        emit checkFailed(reply->errorString());
        return;
    }
    const QByteArray bytes = reply->readAll();
    const std::optional<DataManifest> remote = DataManifestReader::parse(bytes);
    if (!remote.has_value()) {
        qCWarning(dataLog) << "manifest parse failed";
        emit checkFailed(tr("Could not read the dictionaries manifest."));
        return;
    }
    qCDebug(dataLog) << "manifest ok:" << remote->files.size() << "files, build"
                     << remote->buildDate;
    emit checked(*remote, bytes);
}
