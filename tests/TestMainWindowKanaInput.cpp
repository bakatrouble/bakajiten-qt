#include "TestMainWindow.h"

#include <QtTest>

#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>

#include "ui/mainwindow/MainWindow.h"
#include "widgets/table/DictionaryTable.h"

#include <memory>

void TestMainWindow::kanaInputConvertsWhileTyping() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    input->clear();
    // Romaji keystrokes render kana as they land (QTest::keyClicks
    // is ASCII-only — exactly the handled path).
    QTest::keyClicks(input, "ta");
    QCOMPARE(input->text(), QStringLiteral("た"));
    // Pending tail stays raw until it settles.
    QTest::keyClicks(input, "n");
    QCOMPARE(input->text(), QStringLiteral("たn"));
    QTest::keyClicks(input, "a");
    QCOMPARE(input->text(), QStringLiteral("たな"));
    // Backspace deletes the last display character whole — never
    // un-converts kana back to romaji.
    QTest::keyClick(input, Qt::Key_Backspace);
    QCOMPARE(input->text(), QStringLiteral("た"));
    QTest::keyClick(input, Qt::Key_Backspace);
    QCOMPARE(input->text(), QString());
    // Typing resumes converting from the re-synced buffer.
    QTest::keyClicks(input, "a");
    QCOMPARE(input->text(), QStringLiteral("あ"));
    // Uppercase enters lowercase; its tail deletes whole too.
    QTest::keyClick(input, Qt::Key_S, Qt::ShiftModifier);
    QCOMPARE(input->text(), QStringLiteral("あs"));
    QTest::keyClick(input, Qt::Key_Backspace);
    QCOMPARE(input->text(), QStringLiteral("あ"));
    // Hepburn yōon convert like their Kunrei twins.
    input->clear();
    QTest::keyClicks(input, "shashoshu");
    QCOMPARE(input->text(), QStringLiteral("しゃしょしゅ"));
}

void TestMainWindow::kanaInputSearchesConverted() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *status = window->findChild<QLabel *>("statusLabel");
    QVERIFY2(status != nullptr, "status label missing");
    // End to end: romaji in, kana query out (hits land on the dict
    // thread; cold load takes seconds, hence the timeout).
    input->clear();
    QTest::keyClicks(input, "taberu");
    QCOMPARE(input->text(), QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("hits")), 30000);
}

void TestMainWindow::kanaInputModalWhilePending() {
    // A partial mora makes the field modal: arrows, space and
    // return are swallowed (cursor and text frozen), conversion
    // keystrokes and backspace still land, and clicks never move
    // the cursor.
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    input->clear();
    QTest::keyClicks(input, "n");
    QCOMPARE(input->text(), QStringLiteral("n"));
    QCOMPARE(input->cursorPosition(), 1);
    // Arrow keys: swallowed, cursor frozen at the end.
    QTest::keyClick(input, Qt::Key_Left);
    QCOMPARE(input->cursorPosition(), 1);
    QCOMPARE(input->text(), QStringLiteral("n"));
    QTest::keyClick(input, Qt::Key_Home);
    QCOMPARE(input->cursorPosition(), 1);
    // Space and return: swallowed, no text, no search.
    QTest::keyClick(input, Qt::Key_Space);
    QCOMPARE(input->text(), QStringLiteral("n"));
    // Clicking mid-text: swallowed, cursor frozen.
    QTest::mouseClick(input, Qt::LeftButton, Qt::NoModifier, QPoint(2, input->height() / 2));
    QCOMPARE(input->cursorPosition(), 1);
    QCOMPARE(input->text(), QStringLiteral("n"));
    // Conversion keystrokes still land through the modal.
    QTest::keyClicks(input, "a");
    QCOMPARE(input->text(), QStringLiteral("な"));
    // Settled: arrows work again.
    QTest::keyClick(input, Qt::Key_Left);
    QCOMPARE(input->cursorPosition(), 0);
    QTest::keyClick(input, Qt::Key_End);
    QCOMPARE(input->cursorPosition(), 1);
    // Backspace still deletes whole while pending.
    QTest::keyClicks(input, "n");
    QCOMPARE(input->text(), QStringLiteral("なn"));
    QTest::keyClick(input, Qt::Key_Backspace);
    QCOMPARE(input->text(), QStringLiteral("な"));
}

void TestMainWindow::kanaInputPartialMoraSkipsSearch() {
    // A partial mora never fires a search (batch conversion would
    // misread たn as たん): the spy stays quiet past the debounce,
    // and the settling keystroke searches normally.
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    QSignalSpy searched(model, SIGNAL(searchFinished(int, double)));
    // Settled query first (also proves the backend is ready).
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(!searched.isEmpty(), 30000);
    input->clear();
    QTest::qWait(400);
    searched.clear();
    // Partial mora: past the 200ms debounce, still no search.
    QTest::keyClicks(input, "n");
    QCOMPARE(input->text(), QStringLiteral("n"));
    QTest::qWait(600);
    QVERIFY2(searched.isEmpty(), "partial mora fired a search");
    // Settling searches through the same debounce.
    QTest::keyClicks(input, "a");
    QCOMPARE(input->text(), QStringLiteral("な"));
    QTRY_VERIFY_WITH_TIMEOUT(!searched.isEmpty(), 5000);
}
