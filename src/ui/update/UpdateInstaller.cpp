#include "ui/update/UpdateInstaller.h"

#include "util/Archive.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

UpdateInstaller::Payload UpdateInstaller::payloadKind(const QString &assetPath) {
    const QString lower = assetPath.toLower();
    if (lower.endsWith(QStringLiteral(".zip"))) {
        return Payload::Zip;
    }
    if (lower.endsWith(QStringLiteral(".tar.gz")) || lower.endsWith(QStringLiteral(".tgz"))) {
        return Payload::Tarball;
    }
    return Payload::Executable;
}

QString UpdateInstaller::swapScript(const QString &stagedDir, const QString &appDir, qint64 pid) {
    // Waits for our own exit (the exe is locked until then), moves
    // every staged file over the install, relaunches, self-deletes.
    // Plain cmd: tasklist/timeout/move exist on every Win10+ host.
    const QString pidText = QString::number(pid);
    return QStringLiteral("@echo off\r\n"
                          ":wait\r\n"
                          "tasklist /FI \"PID eq %1\" 2>nul | find \"%1\" >nul\r\n"
                          "if not errorlevel 1 (timeout /t 1 /nobreak >nul & goto wait)\r\n"
                          "for %%%%f in (\"%2\\*\") do move /y \"%%%%f\" \"%3\\\"\r\n"
                          "start \"\" \"%3\\bakajiten.exe\"\r\n"
                          "del \"%%~f0\"\r\n")
        .arg(pidText, QDir::toNativeSeparators(stagedDir), QDir::toNativeSeparators(appDir));
}

bool UpdateInstaller::replaceFile(const QString &from, const QString &to, QString &reason) {
    if (QFileInfo(from).absoluteFilePath() == QFileInfo(to).absoluteFilePath()) {
        return true; // Downloaded straight over the running file.
    }
    QFile::remove(to);
    if (!QFile::rename(from, to)) {
        // Cross-volume: rename cannot do it; copy instead.
        if (!QFile::copy(from, to)) {
            reason = QStringLiteral("Cannot replace %1").arg(to);
            return false;
        }
        QFile::remove(from);
    }
    return true;
}

bool UpdateInstaller::plan(const QString &assetPath, Plan &out, QString &reason) {
    const QString appFile = QCoreApplication::applicationFilePath();
    const QString appDir = QCoreApplication::applicationDirPath();
    const bool writable = QFileInfo(appDir).isWritable();
    QDir staging(QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                     .filePath(QStringLiteral("bakajiten-update")));
    // Fresh stage every run: a crashed previous attempt must not
    // linger (and nothing else uses this directory).
    if (staging.exists() && !staging.removeRecursively()) {
        reason = QStringLiteral("Cannot clear %1").arg(staging.path());
        return false;
    }
    if (!QDir().mkpath(staging.path())) {
        reason = QStringLiteral("Cannot create %1").arg(staging.path());
        return false;
    }

    QFile asset(assetPath);
    if (!asset.open(QIODevice::ReadOnly)) {
        reason = QStringLiteral("Cannot read %1").arg(assetPath);
        return false;
    }
    const QByteArray blob = asset.readAll();

    switch (payloadKind(assetPath)) {
    case Payload::Tarball: {
        // linux-static ships one binary at the archive top level.
        QVector<Archive::Entry> entries;
        if (!Archive::readTarGz(blob, entries, reason)) {
            return false;
        }
        if (!Archive::unpackTo(entries, staging.path(), reason)) {
            return false;
        }
        const QString staged = staging.filePath(QFileInfo(appFile).fileName());
        if (!QFileInfo(staged).isFile()) {
            reason = QStringLiteral("Update archive has no %1").arg(QFileInfo(appFile).fileName());
            return false;
        }
        if (writable) {
            if (!replaceFile(staged, appFile, reason)) {
                return false;
            }
            out = {appFile, {}, appFile, true};
            return true;
        }
        out = {staged, {}, staged, false};
        return true;
    }
    case Payload::Zip: {
        QVector<Archive::Entry> entries;
        if (!Archive::readZip(blob, entries, reason)) {
            return false;
        }
        if (!Archive::unpackTo(entries, staging.path(), reason)) {
            return false;
        }
#ifdef Q_OS_WIN
        // The running exe is locked: stage, then swap after exit.
        const QString script = staging.filePath(QStringLiteral("update.cmd"));
        QFile out_script(script);
        if (!out_script.open(QIODevice::WriteOnly | QIODevice::Text)) {
            reason = QStringLiteral("Cannot write %1").arg(script);
            return false;
        }
        out_script.write(
            swapScript(staging.path(), appDir, QCoreApplication::applicationPid()).toUtf8());
        out_script.close();
        out = {QStringLiteral("cmd.exe"), {QStringLiteral("/c"), script}, appDir, false};
        return true;
#elif defined(Q_OS_MACOS)
        // The zip carries the bundle at its top level.
        const QFileInfoList bundles =
            QDir(staging.path()).entryInfoList({QStringLiteral("*.app")}, QDir::Dirs);
        if (bundles.isEmpty()) {
            reason = QStringLiteral("Update archive has no .app bundle");
            return false;
        }
        const QString staged = bundles.first().absoluteFilePath();
        const QString bundle = QDir(appDir + QStringLiteral("/../..")).absolutePath();
        if (writable) {
            const QString backup = bundle + QStringLiteral(".bakajiten-prev");
            // Swap atomically enough: old aside, new in, old dropped.
            if (QFileInfo::exists(backup)) {
                QDir(backup).removeRecursively();
            }
            if (!QDir().rename(bundle, backup) || !QDir().rename(staged, bundle)) {
                // Roll back: put the old bundle back if it moved.
                if (!QFileInfo::exists(bundle) && QFileInfo::exists(backup)) {
                    QDir().rename(backup, bundle);
                }
                reason = QStringLiteral("Cannot replace %1").arg(bundle);
                return false;
            }
            QDir(backup).removeRecursively();
            const QString packageExe = QDir(bundle).filePath(QStringLiteral("Contents/MacOS/") +
                                                             QFileInfo(appFile).fileName());
            QFile::setPermissions(packageExe, QFile::permissions(packageExe) | QFile::ExeOwner |
                                                  QFile::ExeGroup | QFile::ExeOther);
            out = {QStringLiteral("/usr/bin/open"), {bundle}, bundle, true};
            return true;
        }
        // Not writable (e.g. /Applications): run the staged bundle.
        out = {QStringLiteral("/usr/bin/open"), {staged}, staged, false};
        return true;
#else
        Q_UNUSED(writable);
        reason = QStringLiteral("Zip updates are for the Windows and macOS builds");
        return false;
#endif
    }
    case Payload::Executable:
        break;
    }

    // Bare executable (AppImage and friends): replace when the
    // install dir allows it, else run the downloaded copy in place.
    if (writable) {
        if (!replaceFile(assetPath, appFile, reason)) {
            return false;
        }
#ifndef Q_OS_WIN
        QFile::setPermissions(appFile, QFile::permissions(appFile) | QFile::ExeOwner |
                                           QFile::ExeGroup | QFile::ExeOther);
#endif
        out = {appFile, {}, appFile, true};
        return true;
    }
    out = {assetPath, {}, assetPath, false};
    return true;
}
