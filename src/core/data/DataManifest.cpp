#include "core/data/DataManifest.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

std::optional<DataManifest> DataManifestReader::parse(const QByteArray &bytes) {
    const QJsonDocument document = QJsonDocument::fromJson(bytes);
    if (!document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    const QJsonObject files = root.value(QStringLiteral("files")).toObject();
    if (files.isEmpty()) {
        return std::nullopt;
    }
    DataManifest manifest;
    manifest.buildDate = root.value(QStringLiteral("build_date")).toString();
    for (auto it = files.begin(); it != files.end(); ++it) {
        const QJsonObject entry = it.value().toObject();
        DataFileEntry file;
        file.size = static_cast<qint64>(entry.value(QStringLiteral("size")).toDouble());
        file.gz = static_cast<qint64>(entry.value(QStringLiteral("gz")).toDouble());
        file.version = entry.value(QStringLiteral("version")).toString();
        file.sha256 = entry.value(QStringLiteral("sha256")).toString();
        for (const QString &source :
             entry.value(QStringLiteral("sources")).toVariant().toStringList()) {
            file.sources << source;
        }
        if (file.sha256.isEmpty() || file.size <= 0) {
            return std::nullopt;
        }
        manifest.files.insert(it.key(), file);
    }
    return manifest;
}

std::optional<DataManifest> DataManifestReader::readFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return parse(file.readAll());
}

QString DataManifestReader::sha256Of(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        hash.addData(file.read(1 << 20));
    }
    return QString::fromLatin1(hash.result().toHex());
}

QString DataManifestReader::sha256Of(const QByteArray &bytes) {
    const QByteArray digest = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toHex());
}

DataFileStatus DataManifestReader::status(const DataFileEntry &entry, const QString &localPath) {
    if (!QFileInfo::exists(localPath) || QFileInfo(localPath).size() != entry.size) {
        return QFileInfo::exists(localPath) ? DataFileStatus::Outdated : DataFileStatus::Missing;
    }
    return sha256Of(localPath) == entry.sha256 ? DataFileStatus::Current : DataFileStatus::Outdated;
}
