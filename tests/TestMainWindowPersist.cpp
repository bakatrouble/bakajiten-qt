#include "TestMainWindow.h"

#include <QtTest>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMouseEvent>
#include <QSignalSpy>

#include "core/dictionary/DictionaryModel.h"
#include "core/settings/MemorizedState.h"
#include "core/settings/Settings.h"
#include "ui/mainwindow/MainWindow.h"
#include "widgets/table/DictionaryTable.h"
#include "widgets/text/ArticleView.h"

#include <QHeaderView>
#include <QLineEdit>
#include <QScrollBar>
#include <QSplitter>
#include <memory>

void TestMainWindow::dictionaryColumnWidthsPersist() {
    // Resize both fixed columns, reopen, widths restored (meaning
    // column stretches and is layout-owned).
    {
        std::unique_ptr<MainWindow> window(openWindow());
        QVERIFY2(window != nullptr, "window never exposed");
        auto *table = window->findChild<DictionaryTable *>("resultsTable");
        QVERIFY2(table != nullptr, "results table missing or not promoted");
        table->horizontalHeader()->resizeSection(0, 217);
        table->horizontalHeader()->resizeSection(1, 183);
        QCOMPARE(table->horizontalHeader()->sectionSize(0), 217);
        QCOMPARE(table->horizontalHeader()->sectionSize(1), 183);
    }
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    QCOMPARE(table->horizontalHeader()->sectionSize(0), 217);
    QCOMPARE(table->horizontalHeader()->sectionSize(1), 183);
    // Widths survive a search (model resets don't touch them).
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    input->setText(QStringLiteral("あ"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(table->model()->rowCount() > 200, 30000);
    QCOMPARE(table->horizontalHeader()->sectionSize(0), 217);
    QCOMPARE(table->horizontalHeader()->sectionSize(1), 183);
}
void TestMainWindow::dictionaryArticleWidthPersists() {
    // Drag the splitter, reopen, article width restored from the
    // memorized file (same 1100x750 geometry both times).
    int draggedWidth = 0;
    {
        std::unique_ptr<MainWindow> window(openWindow());
        QVERIFY2(window != nullptr, "window never exposed");
        auto *table = window->findChild<DictionaryTable *>("resultsTable");
        QVERIFY2(table != nullptr, "results table missing or not promoted");
        auto *split = window->findChild<QSplitter *>("mainSplit");
        QVERIFY2(split != nullptr, "main splitter missing");
        auto *article = window->findChild<ArticleView *>("articleView");
        QVERIFY2(article != nullptr, "article pane missing");
        auto *input = window->findChild<QLineEdit *>("searchInput");
        QVERIFY2(input != nullptr, "search input missing");
        input->setText(QStringLiteral("あ"));
        QTest::keyClick(input, Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT(table->model()->rowCount() > 200, 30000);
        table->selectRow(0);
        QTRY_VERIFY_WITH_TIMEOUT(article->isVisible(), 5000);
        // The first-show setSizes resizes the children; the handle
        // widget settles on the layout pass — wait until it sits at
        // the division before grabbing it (a stale far-right handle
        // would drag wrong).
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(split->handle(1)->pos().x() - split->sizes().value(0)) <= 3,
                                 5000);
        auto *handle = split->handle(1);
        QVERIFY2(handle != nullptr, "splitter handle missing");
        const QPoint from = handle->rect().center();
        const QPoint to = from + QPoint(-150, 0);
        const int before = article->width();
        QVERIFY(before > 0);
        // Stepped hand-built drag (explicit buttons and global
        // positions — the handle tracks globals; QTest's
        // cursor-moving drag mis-tracks the 1px handle).
        const auto mouseAt = [&](QEvent::Type type, const QPoint &at, Qt::MouseButton button,
                                 Qt::MouseButtons buttons) {
            QMouseEvent event(type, QPointF(at), QPointF(handle->mapToGlobal(at)), button, buttons,
                              Qt::NoModifier);
            QApplication::sendEvent(handle, &event);
        };
        mouseAt(QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
        for (int step = 1; step <= 5; ++step) {
            mouseAt(QEvent::MouseMove, from + (to - from) * step / 5, Qt::NoButton, Qt::LeftButton);
        }
        mouseAt(QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
        QTRY_VERIFY_WITH_TIMEOUT(article->width() != before, 5000);
        draggedWidth = article->width();
        QVERIFY(draggedWidth > before);
        // Live drags persist (never the hidden zero).
        QCOMPARE(MemorizedState::articleWidth(), draggedWidth);
    }
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *article = window->findChild<ArticleView *>("articleView");
    QVERIFY2(article != nullptr, "article pane missing");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    input->setText(QStringLiteral("あ"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(table->model()->rowCount() > 200, 30000);
    table->selectRow(0);
    QTRY_VERIFY_WITH_TIMEOUT(article->isVisible(), 5000);
    QCOMPARE(article->width(), draggedWidth);
    // Leave no trace: later tests assume the default split.
    MemorizedState::clearArticleWidth();
}
void TestMainWindow::dictionaryResizePreservesArticleWidth() {
    // Window resize keeps the article width; the table absorbs the
    // whole delta (resizes never persist, so no cleanup needed).
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *article = window->findChild<ArticleView *>("articleView");
    QVERIFY2(article != nullptr, "article pane missing");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    input->setText(QStringLiteral("あ"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(table->model()->rowCount() > 200, 30000);
    table->selectRow(0);
    QTRY_VERIFY_WITH_TIMEOUT(article->isVisible(), 5000);
    const int articleWidth = article->width();
    QVERIFY(articleWidth > 0);
    const int tableWidth = table->width();
    // Wider: the table proves layout ran, the article holds exact.
    window->resize(1300, 750);
    QTRY_VERIFY_WITH_TIMEOUT(table->width() > tableWidth, 5000);
    QCOMPARE(article->width(), articleWidth);
    // Narrower (still room): same, in the other direction.
    const int wideTable = table->width();
    window->resize(900, 750);
    QTRY_VERIFY_WITH_TIMEOUT(table->width() < wideTable, 5000);
    QCOMPARE(article->width(), articleWidth);
}
void TestMainWindow::dictionarySelectionKeepsTableScroll() {
    // Row selection must not move the horizontal scrollbar:
    // Qt parks current in the last (6000px meaning) column and the
    // view scrolls it into view, yanking long-meaning reads back
    // to the column edge on every selection (including article
    // open). Current stays on the heading column instead.
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *article = window->findChild<ArticleView *>("articleView");
    QVERIFY2(article != nullptr, "article pane missing");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    input->setText(QStringLiteral("あ"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(table->model()->rowCount() > 200, 30000);
    table->selectRow(0);
    QTRY_VERIFY_WITH_TIMEOUT(article->isVisible(), 5000);
    auto *hbar = table->horizontalScrollBar();
    QVERIFY(hbar->maximum() > 2000);
    hbar->setValue(2000);
    QTest::qWait(300);
    QCOMPARE(hbar->value(), 2000);
    // Reselecting (article closes and reopens) holds the scroll
    // and keeps current out of the meaning column.
    table->clearSelections();
    QTRY_VERIFY_WITH_TIMEOUT(!article->isVisible(), 5000);
    hbar->setValue(2000);
    QTest::qWait(300);
    table->selectRow(0);
    QTRY_VERIFY_WITH_TIMEOUT(article->isVisible(), 5000);
    QCOMPARE(table->currentIndex().column(), 0);
    QCOMPARE(hbar->value(), 2000);
}
void TestMainWindow::settingsNestShape() {
    AppSettings stored;
    stored.setScale(1.5);
    stored.setStyle(QStringLiteral("fusion"));
    stored.setIcons(IconScheme::Dark);
    stored.save();
    // On-disk shape: slash-keys nest (`ui:` section).
    QFile file(AppSettings::storagePath());
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString text = QString::fromUtf8(file.readAll());
    QVERIFY2(text.contains(QStringLiteral("ui:")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("scale: 1.5")), qPrintable(text));
    // ...and it loads back exactly.
    QCOMPARE(AppSettings::fromStorage(), stored);
}
void TestMainWindow::settingsFlatFallback() {
    // Pre-nesting files (flat top-level keys) keep loading.
    QFile file(AppSettings::storagePath());
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
    file.write("scale: 1.25\nstyle: windows\nicons: 1\n");
    file.close();
    const AppSettings loaded = AppSettings::fromStorage();
    QVERIFY(qFuzzyCompare(loaded.scale() + 1.0, 2.25));
    QCOMPARE(loaded.style(), QStringLiteral("windows"));
    QCOMPARE(loaded.icons(), IconScheme::Light);
    AppSettings::resetStorage();
}
void TestMainWindow::fontScaleRescalesMemorizedWidths() {
    // Memorized column widths are old-scale pixels: a scale change
    // rescales them once so columns keep their share of the table.
    MemorizedState::setResultsHeadingWidth(100);
    MemorizedState::setResultsReadingWidth(120);
    MemorizedState::setArticleWidth(200);
    MemorizedState::rescaleWidths(1.5);
    QCOMPARE(MemorizedState::resultsHeadingWidth(), 150);
    QCOMPARE(MemorizedState::resultsReadingWidth(), 180);
    QCOMPARE(MemorizedState::articleWidth(), 300);
    // Identity factor is a no-op (never rewrites user values).
    MemorizedState::rescaleWidths(1.0);
    QCOMPARE(MemorizedState::resultsHeadingWidth(), 150);
    QFile::remove(AppSettings::memorizedPath());
}
