#include "TestMainWindow.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QTest>

#include "TestDataDir.h"
#include "core/settings/Settings.h"
#include "core/settings/YamlFormat.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/recognizer/RecognizerWidget.h"

/// Shown top-level window with an exposed panel lookup.
MainWindow *openWindow() {
    auto *window = new MainWindow;
    window->resize(1100, 750);
    window->show();
    if (!QTest::qWaitForWindowExposed(window)) {
        delete window;
        return nullptr;
    }
    return window;
}

RecognizerWidget *panelOf(MainWindow *window) { return window->findChild<RecognizerWidget *>(); }

QPushButton *openButtonOf(MainWindow *window) {
    return window->findChild<QPushButton *>("recognizerButton");
}

void TestMainWindow::initTestCase() {
    // Isolate persisted state: memorized UI values (panel size)
    // live in a temp-adjacent state file, while AppSettings (YAML)
    // points at a temp file.
    QCoreApplication::setOrganizationName(QStringLiteral("bakajiten-test"));
    QCoreApplication::setApplicationName(QStringLiteral("bakajiten-test"));
    QFile::remove(AppSettings::memorizedPath());
    qputenv("BAKAJITEN_SETTINGS",
            (QDir::tempPath() + QStringLiteral("/bakajiten-test-settings.yaml")).toUtf8());
    // Packed repo data (CI downloads it to the repo root): without
    // this the data dir is the temp settings dir and every search
    // test finds nothing.
    pointDataDirAtRepoData();
    AppSettings::resetStorage();
}

void TestMainWindow::cleanupTestCase() {
    QFile::remove(AppSettings::memorizedPath());
    AppSettings::resetStorage();
}

#include "TestDataManifest.h"
#include "TestGzip.h"
#include "TestMainWindow.h"
#include "TestPanel.h"
#include "TestPanelSize.h"

#include <QApplication>

/// One binary, three suites (ex-panel, ex-panelsize, main): an
/// optional first positional arg selects a single suite (ctest
/// entries below); otherwise everything runs, statuses OR-ed.
/// A test-function filter runs only in the suites declaring it
/// (unknown functions used to fail every other suite). Each suite
/// isolates its own settings/state in init/cleanup, so
/// shared-process order is fixed: pure math, panel, main window.
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    int first = 1;
    QString only;
    if (argc > 1) {
        const QString arg = QString::fromLocal8Bit(argv[1]);
        if (arg == QStringLiteral("panelsize") || arg == QStringLiteral("panel") ||
            arg == QStringLiteral("mainwindow") || arg == QStringLiteral("data")) {
            only = arg;
            first = 2;
        }
    }
    // QTest parses its own flags per qExec: hand each suite the
    // args minus the selector.
    const int rest = argc - first;
    QVector<char *> forwarded(rest + 1);
    forwarded[0] = argv[0];
    for (int i = 0; i < rest; ++i) {
        forwarded[i + 1] = argv[first + i];
    }
    // Positional filters select suites by declared slot (flags
    // start with '-'; everything else names a test function).
    QStringList wanted;
    for (int i = first; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (!arg.startsWith(u'-')) {
            wanted << arg.split(u':').first();
        }
    }
    const auto declares = [&](const QObject &suite) {
        if (wanted.isEmpty()) {
            return true;
        }
        const QMetaObject *meta = suite.metaObject();
        for (int i = 0; i < meta->methodCount(); ++i) {
            if (wanted.contains(QString::fromLatin1(meta->method(i).name()))) {
                return true;
            }
        }
        return false;
    };
    int status = 0;
    const auto run = [&](QObject &suite) {
        if (declares(suite)) {
            status |= QTest::qExec(&suite, rest + 1, forwarded.data());
        }
    };
    if (only.isEmpty() || only == QStringLiteral("data")) {
        TestDataManifest data;
        run(data);
        TestGzip gzip;
        run(gzip);
    }
    if (only.isEmpty() || only == QStringLiteral("panelsize")) {
        TestPanelSize size;
        run(size);
    }
    if (only.isEmpty() || only == QStringLiteral("panel")) {
        TestRecognizerPanel panel;
        run(panel);
    }
    if (only.isEmpty() || only == QStringLiteral("mainwindow")) {
        TestMainWindow window;
        run(window);
    }
    return status;
}
#include "TestMainWindow.moc"
