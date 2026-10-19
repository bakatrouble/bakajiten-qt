#pragma once

#include <QString>
#include <QStringList>
#include <cstdint>

/// Installs a downloaded update asset over the running application,
/// then hands back what to launch for the restart.
///
/// Payloads: a bare executable (AppImage / exe sidecar), a .tar.gz
/// holding one binary (linux-static), or a .zip (win64 exe +
/// OFL.txt, macOS .app bundle). Unix replaces in place (a running
/// image may be unlinked); Windows locks the running exe, so the
/// payload stages to a temp dir and a small cmd helper finishes the
/// swap after exit.
class UpdateInstaller {
public:
    enum class Payload : std::uint8_t { Executable, Tarball, Zip };
    static Payload payloadKind(const QString &assetPath);

    struct Plan {
        QString program;       // startDetached target
        QStringList arguments; // startDetached args
        QString installedPath; // replaced (or staged) location, for messages
        bool replacesRunning = false;
    };

    /// Build the install + restart plan, performing the file moves
    /// the platform allows now. False with reason on failure.
    static bool plan(const QString &assetPath, Plan &out, QString &reason);

    /// Windows swap helper body (pure: staged dir, app dir, own pid).
    static QString swapScript(const QString &stagedDir, const QString &appDir, qint64 pid);

private:
    static bool replaceFile(const QString &from, const QString &to, QString &reason);
};
