#include "TestMainWindow.h"

#include <QtTest>

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

#include "core/settings/Settings.h"
#include "ui/mainwindow/MainWindow.h"
#include "widgets/list/CandidateList.h"
#include "widgets/list/CandidateListDelegate.h"

#include "ui/recognizer/RecognizerWidget.h"
#include <memory>

void TestMainWindow::candidateScriptColors() {
    // Script mapping is pure (mirrors the delegate oracle).
    QCOMPARE(CandidateListDelegate::scriptOf(QStringLiteral("あ")),
             CandidateListDelegate::Script::Hiragana);
    QCOMPARE(CandidateListDelegate::scriptOf(QStringLiteral("ア")),
             CandidateListDelegate::Script::Katakana);
    QCOMPARE(CandidateListDelegate::scriptOf(QStringLiteral("字")),
             CandidateListDelegate::Script::Other);
    QCOMPARE(CandidateListDelegate::scriptOf(QString()), CandidateListDelegate::Script::Other);
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    QTest::mouseClick(openButtonOf(window.get()), Qt::LeftButton);
    auto *panel = panelOf(window.get());
    auto *canvas = panel->findChild<QWidget *>("canvas");
    QVERIFY2(canvas != nullptr, "canvas missing");
    const QPoint from(40, 40);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, from);
    QTest::mouseMove(canvas, from + QPoint(60, 0));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, from + QPoint(60, 0));
    auto *list = panel->findChild<QListWidget *>("candidatesList");
    QVERIFY2(list != nullptr, "candidates list missing");
    // Scoring runs on the match thread: wait for real cells (a
    // mixed strip from the model exercises both script colors).
    QTRY_VERIFY(list->count() > 0);
    // The is_kana flag itself is pinned in Rust (ffi tests); the
    // kana script ranges here are only the oracle for which cells
    // must carry script colors. Needs a mixed strip from the model.
    int kata = -1;
    int hira = -1;
    for (int i = 0; i < list->count(); ++i) {
        const QString text = list->item(i)->text();
        if (text.isEmpty()) {
            continue;
        }
        const ushort first = text.at(0).unicode();
        if (kata < 0 && first >= 0x30a0 && first <= 0x30ff) {
            kata = i;
        }
        if (hira < 0 && first >= 0x3040 && first <= 0x309f) {
            hira = i;
        }
    }
    QVERIFY2(kata >= 0, "no katakana hits to check colors against");
    QVERIFY2(hira >= 0, "no hiragana hits to check colors against");
    QApplication::processEvents();
    const QImage shot = list->viewport()->grab().toImage();
    const auto cellPixel = [&](int row, bool corner) {
        const QRect cell = list->visualRect(list->model()->index(row, 0));
        return shot.pixelColor(corner ? cell.topLeft() + QPoint(3, 3) : cell.center());
    };
    // Script containers: blue katakana, green hiragana (hue bands,
    // theme-independent); corners carry the fill, centers the text.
    const double kataHue = cellPixel(kata, true).hslHueF();
    QVERIFY2(kataHue > 0.5 && kataHue < 0.72, "katakana cell is not blue");
    const double hiraHue = cellPixel(hira, true).hslHueF();
    QVERIFY2(hiraHue > 0.22 && hiraHue < 0.45, "hiragana cell is not green");
    // Hover inverts the scheme: the corner fill becomes the
    // inverted tone exactly (flat fill, no glyph dependence).
    const QColor kataFill = cellPixel(kata, true);
    QTest::mouseMove(list->viewport(), list->visualRect(list->model()->index(kata, 0)).center());
    QTest::qWait(100);
    const QImage hovered = list->viewport()->grab().toImage();
    const QRect kataCell = list->visualRect(list->model()->index(kata, 0));
    QColor invertedBg;
    QColor invertedFg;
    CandidateListDelegate::cellColors(CandidateListDelegate::Script::Katakana, true,
                                      list->palette(), invertedBg, invertedFg);
    QCOMPARE(hovered.pixelColor(kataCell.topLeft() + QPoint(3, 3)).rgb(), invertedBg.rgb());
    QVERIFY(invertedBg.rgb() != kataFill.rgb());
}

void TestMainWindow::candidateClickInsertsQuery() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    QTest::mouseClick(openButtonOf(window.get()), Qt::LeftButton);
    auto *list = panelOf(window.get())->findChild<QListWidget *>("candidatesList");
    QVERIFY2(list != nullptr, "candidates list missing");
    list->addItem(QStringLiteral("あ"));
    QApplication::processEvents();
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    // Plain click (no drag): the text lands in the search field and
    // searches. Starts from an empty field so the append lands
    // exactly (the widget ships a default query).
    input->clear();
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                      list->visualRect(list->model()->index(0, 0)).center());
    QCOMPARE(input->text(), QStringLiteral("あ"));
    auto *status = window->findChild<QLabel *>("statusLabel");
    QVERIFY2(status != nullptr, "status label missing");
    // Real backend on the dict thread: rows (and the hit count)
    // land a queued delivery later. Cold artifact load (hundreds of
    // MB) can take seconds, hence the generous timeout.
    QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("hits")), 30000);
}

void TestMainWindow::candidateInsertsAtCursor() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    QTest::mouseClick(openButtonOf(window.get()), Qt::LeftButton);
    auto *list = panelOf(window.get())->findChild<QListWidget *>("candidatesList");
    QVERIFY2(list != nullptr, "candidates list missing");
    list->addItem(QStringLiteral("あ"));
    QApplication::processEvents();
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    // Mid-text cursor: the pick lands there, not at the end, and
    // focus returns to the field for continued typing.
    input->setText(QStringLiteral("かき"));
    input->setCursorPosition(1);
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                      list->visualRect(list->model()->index(0, 0)).center());
    QCOMPARE(input->text(), QStringLiteral("かあき"));
    QCOMPARE(input->cursorPosition(), 2);
    QVERIFY(input->hasFocus());
}

void TestMainWindow::candidatesFollowScale() {
    // Normalize first (a failed earlier test can leave 1.5 applied).
    AppSettings baseline;
    baseline.setScale(1.0);
    baseline.apply();
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    QTest::mouseClick(openButtonOf(window.get()), Qt::LeftButton);
    auto *list = panelOf(window.get())->findChild<QListWidget *>("candidatesList");
    QVERIFY2(list != nullptr, "candidates list missing");
    // Twice the interface font, rows fitted to it — never fixed.
    QVERIFY(qFuzzyCompare(list->font().pointSizeF(), QApplication::font().pointSizeF() * 2.0));
    const int shortRow = list->minimumHeight();
    QVERIFY(shortRow > 0);
    // Live interface-font changes re-derive (same path as builds).
    AppSettings up;
    up.setScale(1.5);
    up.apply();
    QTest::qWait(100);
    QVERIFY(list->font().pointSizeF() > QApplication::font().pointSizeF());
    QVERIFY(list->minimumHeight() > shortRow);
    baseline.apply();
}

void TestMainWindow::candidateScaleStaysIdempotent() {
    // The list font derives from the interface font (never its own
    // explicit one): repeat calls never compound 2x into 4x.
    CandidateList list;
    list.refreshScale();
    const double once = list.font().pointSizeF();
    QVERIFY(once > QApplication::font().pointSizeF());
    list.refreshScale();
    QCOMPARE(list.font().pointSizeF(), once);
}
