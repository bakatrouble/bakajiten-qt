#include "TestMainWindow.h"

#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QTimer>

#include "ui/mainwindow/MainWindow.h"

#include "ui/recognizer/RecognizerWidget.h"
#include "widgets/icons/ButtonIcon.h"
#include <QPushButton>
#include <QScrollBar>
#include <memory>

void TestMainWindow::panelHiddenByDefault() {
    MainWindow window;
    window.resize(1100, 750);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *panel = panelOf(&window);
    QVERIFY2(panel != nullptr, "recognizer panel missing");
    QVERIFY(!panel->isVisible());
}

void TestMainWindow::openButtonShowsPanel() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *button = openButtonOf(window.get());
    QVERIFY2(button != nullptr, "open button missing");
    QTest::mouseClick(button, Qt::LeftButton);
    QVERIFY(panelOf(window.get())->isVisible());
}

void TestMainWindow::openButtonTogglesPanelClosed() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *button = openButtonOf(window.get());
    QVERIFY2(button != nullptr, "open button missing");
    QTest::mouseClick(button, Qt::LeftButton);
    QVERIFY(panelOf(window.get())->isVisible());
    QVERIFY(button->isChecked());
    // Second click unchecks the toggle: the panel closes with it.
    QTest::mouseClick(button, Qt::LeftButton);
    QVERIFY(!panelOf(window.get())->isVisible());
    QVERIFY(!button->isChecked());
}

void TestMainWindow::panelOpensUnderButton() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *button = openButtonOf(window.get());
    QVERIFY2(button != nullptr, "open button missing");
    QTest::mouseClick(button, Qt::LeftButton);
    auto *panel = panelOf(window.get());
    QVERIFY(panel->isVisible());
    // Panel top-left lands on the button's bottom-left, clamped
    // into the central widget (mirrors setRecognizerVisible).
    auto *central = window->centralWidget();
    const QPoint anchor = button->mapTo(central, QPoint(0, button->height()));
    const int x = qBound(0, anchor.x(), qMax(0, central->width() - panel->width()));
    const int y = qBound(0, anchor.y(), qMax(0, central->height() - panel->height()));
    QCOMPARE(panel->pos(), QPoint(x, y));
}

void TestMainWindow::clickAwayHidesPanel() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    QTest::mouseClick(openButtonOf(window.get()), Qt::LeftButton);
    QVERIFY(panelOf(window.get())->isVisible());
    // Click into the results table: outside the panel, the button,
    // and the search field (clicks there only move the cursor now).
    auto *table = window->findChild<QWidget *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing");
    QTest::mouseClick(table, Qt::LeftButton, Qt::NoModifier,
                      QPoint(table->width() / 2, table->height() / 2));
    QVERIFY(!panelOf(window.get())->isVisible());
}

void TestMainWindow::panelOpenFocusesInput() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    QTest::mouseClick(openButtonOf(window.get()), Qt::LeftButton);
    QVERIFY(panelOf(window.get())->isVisible());
    // Typing continues while drawing: the query field takes focus
    // on open (the auto-hide policy exempts it).
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    QVERIFY(input->hasFocus());
}

void TestMainWindow::searchFieldClickKeepsPanel() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    QTest::mouseClick(openButtonOf(window.get()), Qt::LeftButton);
    QVERIFY(panelOf(window.get())->isVisible());
    // Clicking the field only moves the cursor: neither the press
    // nor the focus move may dismiss the panel.
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    QTest::mouseClick(input, Qt::LeftButton, Qt::NoModifier, QPoint(20, input->height() / 2));
    QVERIFY(panelOf(window.get())->isVisible());
    QVERIFY(input->hasFocus());
}

void TestMainWindow::helpMenuShowsAbout() {
    // Help → About opens the app info box (closed by the test).
#ifdef Q_OS_MAC
    QSKIP("native alerts are not QWidgets: no title to read, and the "
          "close timer would outlive the window");
#endif
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *action = window->findChild<QAction *>("actionAbout");
    QVERIFY2(action != nullptr, "about action missing");
    QString title;
    QString text;
    bool linksOpen = false;
    QTimer::singleShot(1000, [&] {
        if (auto *box = window->findChild<QMessageBox *>()) {
            title = box->windowTitle();
            text = box->text();
            for (auto *label : box->findChildren<QLabel *>()) {
                linksOpen = linksOpen || label->openExternalLinks();
            }
            box->accept();
        }
    });
    action->trigger();
    QCOMPARE(title, QStringLiteral("About bakajiten"));
    QVERIFY(text.contains(QStringLiteral("JMdict")));
    QVERIFY(text.contains(QStringLiteral("Sólyom")));
    QVERIFY(text.contains(QStringLiteral("bakatrouble")));
    QVERIFY(text.contains(QCoreApplication::applicationVersion()));
    QVERIFY2(linksOpen, "about links do not open externally");
    // Every bundled source is credited with a working link.
    for (const QString &link :
         {QStringLiteral("http://www.edrdg.org/"), QStringLiteral("https://kanjivg.tagaini.net/"),
          QStringLiteral("https://kaikki.org/jawiktionary/rawdata.html"),
          QStringLiteral("https://bond-lab.github.io/wnja/"),
          QStringLiteral("http://www.tanos.co.uk/jlpt/"),
          QStringLiteral("https://github.com/z1dev/zkanji"),
          QStringLiteral("https://github.com/bakatrouble/")}) {
        QVERIFY2(text.contains(link), qPrintable(link));
    }
}

void TestMainWindow::helpMenuShowsAboutQt() {
    // Help → About Qt opens Qt's info box (closed by the test).
#ifdef Q_OS_MAC
    QSKIP("native alerts are not QWidgets: no title to read, and the "
          "close timer would outlive the window");
#endif
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *action = window->findChild<QAction *>("actionAboutQt");
    QVERIFY2(action != nullptr, "about-qt action missing");
    QString title;
    QTimer::singleShot(1000, [&] {
        if (auto *box = window->findChild<QMessageBox *>()) {
            title = box->windowTitle();
            box->accept();
        }
    });
    action->trigger();
    QVERIFY(title.contains(QStringLiteral("Qt")));
}

void TestMainWindow::fadesFollowScrollability() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    QTest::mouseClick(openButtonOf(window.get()), Qt::LeftButton);
    auto *panel = panelOf(window.get());
    QVERIFY(panel->isVisible());
    auto *list = panel->findChild<QListWidget *>("candidatesList");
    QVERIFY2(list != nullptr, "candidates list missing");
    auto *left = panel->findChild<QWidget *>("leftFade");
    auto *right = panel->findChild<QWidget *>("rightFade");
    QVERIFY2(left != nullptr && right != nullptr, "fade hints missing");
    for (int i = 0; i < 30; ++i) {
        list->addItem(QString("item %1").arg(i));
    }
    QApplication::processEvents();
    auto *bar = list->horizontalScrollBar();
    QVERIFY(bar->maximum() > 0);
    QVERIFY(!left->isVisible());
    QVERIFY(right->isVisible());
    bar->setValue(bar->maximum());
    QVERIFY(left->isVisible());
    QVERIFY(!right->isVisible());
}

void TestMainWindow::queryTimeShownAfterStroke() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    QTest::mouseClick(openButtonOf(window.get()), Qt::LeftButton);
    auto *panel = panelOf(window.get());
    auto *canvas = panel->findChild<QWidget *>("canvas");
    QVERIFY2(canvas != nullptr, "canvas missing");
    auto *status = panel->findChild<QLabel *>("statusLabel");
    QVERIFY2(status != nullptr, "status label missing");
    QVERIFY(!status->text().contains("ms"));
    const QPoint from(40, 40);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, from);
    QTest::mouseMove(canvas, from + QPoint(60, 0));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, from + QPoint(60, 0));
    // Scoring runs on the match thread: cells (and the ms readout)
    // land a queued delivery later.
    QTRY_VERIFY(status->text().contains("ms"));
}

void TestMainWindow::toolbarSquareIcons() {
    // Square SVG toolbar icons (texts live in tooltips): every
    // search-row and panel button carries a 24px icon and no text.
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    const auto check = [&](const char *name, const char *icon) {
        auto *button = window->findChild<QPushButton *>(QString::fromLatin1(name));
        QVERIFY2(button != nullptr, name);
        QVERIFY2(!button->icon().isNull(), name);
        QCOMPARE(button->iconSize(), QSize(ButtonIcon::sidePx, ButtonIcon::sidePx));
        QVERIFY2(button->text().isEmpty(), name);
        QVERIFY2(!button->toolTip().isEmpty(), name);
        QCOMPARE(button->property("svgIcon").toString(), QString::fromLatin1(icon));
    };
    check("suffixButton", "suffix");
    check("prefixButton", "prefix");
    check("recognizerButton", "pen");
    check("japaneseButton", "kana");
    check("meaningButton", "latin");
    check("closeButton", "close");
    check("gridButton", "grid");
    check("kanaButton", "kana-match");
    check("undoButton", "undo");
    check("redoButton", "redo");
    check("clearButton", "erase");
    // Every referenced file ships square in the resource bundle.
    for (const QString &name :
         {QStringLiteral("suffix"), QStringLiteral("prefix"), QStringLiteral("pen"),
          QStringLiteral("kana"), QStringLiteral("kana-match"), QStringLiteral("latin"),
          QStringLiteral("close"), QStringLiteral("grid"), QStringLiteral("undo"),
          QStringLiteral("redo"), QStringLiteral("erase")}) {
        QVERIFY2(QFile::exists(QStringLiteral(":/icons/") + name + QStringLiteral(".svg")),
                 qPrintable(name));
    }
}
