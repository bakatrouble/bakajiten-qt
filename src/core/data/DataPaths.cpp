#include "core/data/DataPaths.h"

#include "core/settings/Settings.h"

#include <QDir>
#include <QFileInfo>

QString DataPaths::directory() {
    if (qEnvironmentVariableIsSet("BAKAJITEN_DATA_DIR")) {
        return QString::fromLocal8Bit(qgetenv("BAKAJITEN_DATA_DIR"));
    }
    return QFileInfo(AppSettings::storagePath()).absolutePath();
}

void DataPaths::ensureDirectory() { QDir().mkpath(directory()); }

QString DataPaths::filePath(const QString &file) { return QDir(directory()).filePath(file); }

QString DataPaths::manifestPath() { return filePath(QStringLiteral("manifest.json")); }

QStringList DataPaths::requiredFiles() {
    return {QStringLiteral("jmdict.bin"), QStringLiteral("kanji.bin"),
            QStringLiteral("templates.bin")};
}

QStringList DataPaths::optionalFiles() {
    return {QStringLiteral("jmnedict.bin"), QStringLiteral("kokugo.bin")};
}

QStringList DataPaths::allFiles() { return requiredFiles() + optionalFiles(); }

bool DataPaths::isOptional(const QString &file) { return optionalFiles().contains(file); }

QString DataPaths::displayName(const QString &file) {
    if (file == QStringLiteral("jmdict.bin")) {
        return QStringLiteral("English (JMdict)");
    }
    if (file == QStringLiteral("jmnedict.bin")) {
        return QStringLiteral("Names (JMnedict)");
    }
    if (file == QStringLiteral("kokugo.bin")) {
        return QStringLiteral("Kokugo");
    }
    if (file == QStringLiteral("kanji.bin")) {
        return QStringLiteral("Kanji");
    }
    if (file == QStringLiteral("templates.bin")) {
        return QStringLiteral("Handwriting");
    }
    return file;
}
