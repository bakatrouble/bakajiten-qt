#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QIcon>

#include "ui/mainwindow/MainWindow.h"

#include "core/settings/Settings.h"

#ifdef BAKAJITEN_STATIC_QT
#include <QtPlugin>
#ifdef Q_OS_WIN
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#else
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#endif
#ifdef BAKAJITEN_STATIC_TLS
Q_IMPORT_PLUGIN(QTlsBackendOpenSSL)
#endif
#endif

int main(int argc, char *argv[]) {
    // Qt drops log messages when stderr is not a console — which is
    // every headless/Xvfb run with redirected output. Force them to
    // stderr instead of losing them silently (read lazily per
    // message, so setting it here is early enough).
    qputenv("QT_FORCE_STDERR_LOGGING", "1");
    QApplication app(argc, argv);
    // Windows builds compile in a CJK fallback (Noto Sans JP); other
    // builds skip it and the resource lookup below misses
    // harmlessly. Loaded unconditionally when present: it only ever
    // renders when nothing else covers the text.
    QString fallbackFamily;
    {
        const QString bundled = QStringLiteral(":/fonts/NotoSansJP.ttf");
        if (QFile::exists(bundled)) {
            const int id = QFontDatabase::addApplicationFont(bundled);
            fallbackFamily = QFontDatabase::applicationFontFamilies(id).value(0);
        }
    }
    // Window + taskbar icon (embedded resource, works installed,
    // bundled, and from the build tree alike).
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/app.svg")));
    // Stable home for QSettings (panel size et al.): without these
    // the paths are undefined per-platform.
    QCoreApplication::setOrganizationName(QStringLiteral("bakajiten"));
    QCoreApplication::setApplicationName(QStringLiteral("bakajiten"));
    // Shown in Help → About; keep in sync with project() VERSION.
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    // Persisted appearance before any widget exists (cleanest
    // polish). The OS style is captured first so the settings
    // Default entry can name it.
    AppSettings::noteOsStyle();
    AppSettings::shared().apply();
    // Explicit fallback chain: Qt's automatic font merging relies on
    // OS font linking, which is broken in some setups (e.g. Wine's
    // Tahoma claims no usable Japanese fallback), leaving tofu even
    // when Japanese-capable fonts are installed. If the applied app
    // font cannot render kana, append a known-good family —
    // preferably the embedded Noto, else the first installed family
    // covering Japanese — so every widget falls back correctly.
    {
        const QFont current = QApplication::font();
        if (!QFontMetricsF(current).inFont(QChar(0x3042))) {
            QString fallback = fallbackFamily;
            if (fallback.isEmpty()) {
                const QFontDatabase db;
                for (const QString &family : db.families()) {
                    if (db.writingSystems(family).contains(QFontDatabase::Japanese)) {
                        fallback = family;
                        break;
                    }
                }
            }
            if (!fallback.isEmpty()) {
                QStringList families = current.families();
                if (!families.contains(current.family())) {
                    families.prepend(current.family());
                }
                if (!families.contains(fallback)) {
                    families.append(fallback);
                }
                QFont fixed = current;
                fixed.setFamilies(families);
                QApplication::setFont(fixed);
            }
        }
    }
    // Scale changes recreate the window in-process: each loop
    // turn builds a fresh one (the old died with its scope).
    int rc = 0;
    do {
        MainWindow window;
        window.show();
        rc = app.exec();
    } while (MainWindow::consumeRecreateRequested());
    return rc;
}
