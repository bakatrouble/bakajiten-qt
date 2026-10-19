#include "ui/update/UpdateChecker.h"

#include "core/settings/MemorizedState.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QVersionNumber>

namespace {
constexpr char releasesUrl[] = "https://api.github.com/repos/bakatrouble/bakajiten/releases/latest";
} // namespace

UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent) {
    network = new QNetworkAccessManager(this);
}

void UpdateChecker::check(bool manual) {
    // Artifact kind is a compile-time parameter (appimage/win64);
    // unset builds never phone home.
    const QString artifact = QStringLiteral(BAKAJITEN_UPDATE_ARTIFACT);
    if (artifact.isEmpty()) {
        return;
    }
    this->manual = manual;
    QNetworkRequest request(QUrl(QString::fromLatin1(releasesUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("bakajiten-update-check"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    connect(network->get(request), &QNetworkReply::finished, this, &UpdateChecker::onReply);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool UpdateChecker::isNewer(const QString &current, const QString &tag) {
    QString stripped = tag;
    if (stripped.startsWith(QLatin1Char('v')) || stripped.startsWith(QLatin1Char('V'))) {
        stripped = stripped.mid(1);
    }
    const QVersionNumber have = QVersionNumber::fromString(current);
    const QVersionNumber latest = QVersionNumber::fromString(stripped);
    return !have.isNull() && !latest.isNull() && latest > have;
}

QString UpdateChecker::matchedAsset(const QStringList &names, const QString &artifact) {
    QString suffix;
    if (artifact == QStringLiteral("appimage")) {
        suffix = QStringLiteral(".AppImage");
    } else if (artifact == QStringLiteral("win64")) {
        suffix = QStringLiteral(".exe");
    } else if (artifact == QStringLiteral("linux-static")) {
        suffix = QStringLiteral(".tar.gz");
    } else if (artifact == QStringLiteral("macos-arm64")) {
        suffix = QStringLiteral("macos-arm64.zip");
    } else if (artifact == QStringLiteral("macos-x64")) {
        suffix = QStringLiteral("macos-x64.zip");
    } else {
        return {};
    }
    for (const QString &name : names) {
        if (name.endsWith(suffix, Qt::CaseInsensitive)) {
            return name;
        }
    }
    return {};
}

void UpdateChecker::onReply() {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply == nullptr) {
        return;
    }
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        // Offline, rate-limited, or no TLS in the bundle: only
        // manual checks report it.
        if (manual) {
            emit checkFailed(reply->errorString());
        }
        return;
    }
    const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
    const QString tag = release.value(QStringLiteral("tag_name")).toString();
    const QString current = QCoreApplication::applicationVersion();
    if (!isNewer(current, tag)) {
        if (manual) {
            emit upToDate();
        }
        return;
    }
    QString version = tag;
    if (version.startsWith(QLatin1Char('v')) || version.startsWith(QLatin1Char('V'))) {
        version = version.mid(1);
    }
    // "Do not notify about this version" survives restarts; even
    // manual checks respect it (uncheck via... checking again never
    // re-arms — the next version notifies fresh).
    if (MemorizedState::skippedUpdateVersion() == version) {
        return;
    }
    const QString notes = release.value(QStringLiteral("body")).toString();
    QStringList names;
    const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue asset : assets) {
        names << asset.toObject().value(QStringLiteral("name")).toString();
    }
    const QString match = matchedAsset(names, QStringLiteral(BAKAJITEN_UPDATE_ARTIFACT));
    if (match.isEmpty()) {
        emit noArtifact(version, notes);
        return;
    }
    QString url;
    qint64 size = 0;
    for (const QJsonValue asset : assets) {
        const QJsonObject object = asset.toObject();
        if (object.value(QStringLiteral("name")).toString() == match) {
            url = object.value(QStringLiteral("browser_download_url")).toString();
            size = object.value(QStringLiteral("size")).toVariant().toLongLong();
            break;
        }
    }
    emit updateFound(version, notes, match, url, size);
}
