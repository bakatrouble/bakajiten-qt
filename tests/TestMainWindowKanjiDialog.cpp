#include "TestMainWindow.h"

#include <QtTest>

#include "widgets/list/CandidateList.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCursor>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>

#include "core/dictionary/DictionaryModel.h"
#include "core/dictionary/KanjiInfo.h"
#include "core/settings/MemorizedState.h"
#include "ui/dictionary/DictionaryWidget.h"
#include "ui/kanji/KanjiDialog.h"
#include "ui/kanji/KanjiStrokeView.h"
#include "ui/mainwindow/MainWindow.h"
#include "widgets/table/DictionaryTable.h"
#include "widgets/text/ArticleView.h"
#include "widgets/text/RubyLabel.h"
#include "widgets/text/RunLabel.h"

#include "ui/recognizer/RecognizerWidget.h"

#include <memory>

namespace {
/// Real v6 kanji artifact (same candidate roots as the app's
/// artifact search).
QString kanjiArtifact() {
    for (const QString &candidate :
         {QStringLiteral("data/kanji.bin"), QStringLiteral("../data/kanji.bin"),
          QStringLiteral("../../data/kanji.bin"), QStringLiteral("../../../data/kanji.bin")}) {
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}
} // namespace

void TestMainWindow::kanjiDialogOpensFromTable() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    // Double-click the 食 glyph (proven hover point): the dialog
    // opens with the stroke count in the title.
    const QRect cell = table->visualRect(model->index(0, 0));
    QVERIFY(!cell.isNull());
    QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                       cell.topLeft() + QPoint(6, cell.height() / 2));
    auto *dialog = window->findChild<KanjiDialog *>("kanjiDialog");
    QVERIFY2(dialog != nullptr, "kanji dialog missing");
    QTRY_VERIFY_WITH_TIMEOUT(dialog->isVisible(), 5000);
    QCOMPARE(dialog->character(), QStringLiteral("食"));
    QVERIFY(dialog->windowTitle().contains(QStringLiteral("Stroke count: 9")));
    // Geometry made it into the diagram, meanings into the panel.
    auto *view = dialog->findChild<KanjiStrokeView *>("kanjiStrokeView");
    QVERIFY2(view != nullptr, "stroke view missing");
    QCOMPARE(view->strokeCount(), 9);
    auto *info = dialog->findChild<RunLabel *>("kanjiInfo");
    QVERIFY2(info != nullptr, "info block missing");
    QVERIFY(info->text().contains(QStringLiteral("eat")));
}

void TestMainWindow::kanjiDialogOpensFromArticle() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *article = window->findChild<ArticleView *>("articleView");
    QVERIFY2(article != nullptr, "article pane missing");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    table->selectRow(0);
    QTRY_VERIFY_WITH_TIMEOUT(article->isVisible(), 5000);
    auto *heading = article->findChild<RubyLabel *>(QStringLiteral("articleHeading"));
    QVERIFY2(heading != nullptr, "article heading missing");
    const int kanjiIndex = heading->text().indexOf(QChar(u'食'));
    QVERIFY2(kanjiIndex >= 0, "no 食 in article heading");
    QTest::mouseDClick(heading, Qt::LeftButton, Qt::NoModifier,
                       heading->characterRect(kanjiIndex).center());
    auto *dialog = window->findChild<KanjiDialog *>("kanjiDialog");
    QVERIFY2(dialog != nullptr, "kanji dialog missing");
    QTRY_VERIFY_WITH_TIMEOUT(dialog->isVisible(), 5000);
    QCOMPARE(dialog->character(), QStringLiteral("食"));
}

void TestMainWindow::kanjiDialogIgnoresKana() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    // Double-clicking kana opens nothing.
    const QRect reading = table->visualRect(model->index(0, 1));
    QVERIFY(!reading.isNull());
    QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, reading.center());
    QTest::qWait(300);
    QVERIFY(window->findChild<KanjiDialog *>("kanjiDialog") == nullptr);
}

void TestMainWindow::kanjiDialogCaptionsAreBold() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    const QRect cell = table->visualRect(model->index(0, 0));
    QVERIFY(!cell.isNull());
    QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                       cell.topLeft() + QPoint(6, cell.height() / 2));
    auto *dialog = window->findChild<KanjiDialog *>("kanjiDialog");
    QVERIFY2(dialog != nullptr, "kanji dialog missing");
    QTRY_VERIFY_WITH_TIMEOUT(dialog->isVisible(), 5000);
    // One rich-text block: every paragraph leads with its bold
    // caption, the value follows on the same line.
    auto *info = dialog->findChild<RunLabel *>(QStringLiteral("kanjiInfo"));
    QVERIFY2(info != nullptr, "info block missing");
    QVERIFY(info->text().startsWith(QStringLiteral("Meanings: ")));
    int paragraphs = 0;
    for (int p = 0;; ++p) {
        const auto formats = info->formatsForParagraph(p);
        if (formats.isEmpty()) {
            break;
        }
        QVERIFY2(formats.first().format.font().bold(), "caption not bold");
        ++paragraphs;
    }
    QVERIFY2(paragraphs > 20, "info block lost rows");
}

void TestMainWindow::kanjiDialogShowsFullDetail() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    const QRect cell = table->visualRect(model->index(0, 0));
    QVERIFY(!cell.isNull());
    QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                       cell.topLeft() + QPoint(6, cell.height() / 2));
    auto *dialog = window->findChild<KanjiDialog *>("kanjiDialog");
    QVERIFY2(dialog != nullptr, "kanji dialog missing");
    QTRY_VERIFY_WITH_TIMEOUT(dialog->isVisible(), 5000);
    QCOMPARE(dialog->character(), QStringLiteral("食"));
    // Every kanjidic row lands in the block (`-` when the source
    // carries none).
    auto *info = dialog->findChild<RunLabel *>(QStringLiteral("kanjiInfo"));
    QVERIFY2(info != nullptr, "info block missing");
    const QString text = info->text();
    for (const char *caption :
         {"Meanings: ",    "ON readings: ", "Kun readings: ",  "Nanori: ",
          "Chinese: ",     "Korean: ",      "Vietnamese: ",    "Grade: ",
          "JLPT: ",        "Frequency: ",   "Radical names: ", "Strokes: ",
          "JIS X0208: ",   "JIS X0212: ",   "JIS X0213: ",     "JIS X0208 refs: ",
          "Unicode: ",     "SKIP: ",        "Four corner: ",   "De Roo: ",
          "SH: ",          "Nelson: ",      "NJECD: ",         "O'Neill: ",
          "De Roo refs: ", "SH refs: "}) {
        QVERIFY2(text.contains(QString::fromLatin1(caption)), "detail row missing");
    }
    // Spot values from packed kanjidic2: SKIP `n-nn-nn` shape,
    // news frequency rank present, Chinese in marked pinyin
    // (shì, not shi4).
    const int skipAt = text.indexOf(QStringLiteral("SKIP: "));
    QVERIFY2(skipAt >= 0, "SKIP row missing");
    QVERIFY2(text.mid(skipAt, 14).contains(QChar(u'-')), "SKIP shape wrong");
    QVERIFY2(text.contains(QStringLiteral("Frequency: ")), "frequency row missing");
    QVERIFY2(text.contains(QStringLiteral("shí")), "Chinese not marked pinyin");
}

void TestMainWindow::kanjiDialogSpanSelectsDetails() {
    // Press-drag across rows selects the whole span (captions
    // included); one copy covers it — all native to the widget.
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    const QRect cell = table->visualRect(model->index(0, 0));
    QVERIFY(!cell.isNull());
    QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                       cell.topLeft() + QPoint(6, cell.height() / 2));
    auto *dialog = window->findChild<KanjiDialog *>("kanjiDialog");
    QVERIFY2(dialog != nullptr, "kanji dialog missing");
    QTRY_VERIFY_WITH_TIMEOUT(dialog->isVisible(), 5000);
    auto *info = dialog->findChild<RunLabel *>(QStringLiteral("kanjiInfo"));
    QVERIFY2(info != nullptr, "info block missing");
    QTRY_VERIFY_WITH_TIMEOUT(info->height() > 100, 5000);
    const QPoint from(25, 12);
    QVERIFY(info->characterAt(from) > 0);
    QTest::mousePress(info, Qt::LeftButton, Qt::NoModifier, from);
    const QPoint to(25, 150);
    QVERIFY(info->characterAt(to) >= 0);
    QTest::mouseMove(info, to);
    QTest::mouseRelease(info, Qt::LeftButton, Qt::NoModifier, to);
    const QString picked = info->selectedText();
    QVERIFY(!picked.isEmpty());
    QVERIFY2(picked.contains(QChar(u'\n')), "span never left its row");
    QTest::keyClick(info, Qt::Key_C, Qt::ControlModifier);
    QTRY_VERIFY_WITH_TIMEOUT(QApplication::clipboard()->text() == picked, 5000);
}

void TestMainWindow::kanjiDialogCaptionSelects() {
    // Captions select like values: a drag inside the caption picks
    // up caption text (never value text).
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    const QRect cell = table->visualRect(model->index(0, 0));
    QVERIFY(!cell.isNull());
    QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                       cell.topLeft() + QPoint(6, cell.height() / 2));
    auto *dialog = window->findChild<KanjiDialog *>("kanjiDialog");
    QVERIFY2(dialog != nullptr, "kanji dialog missing");
    QTRY_VERIFY_WITH_TIMEOUT(dialog->isVisible(), 5000);
    auto *info = dialog->findChild<RunLabel *>(QStringLiteral("kanjiInfo"));
    QVERIFY2(info != nullptr, "info block missing");
    QTRY_VERIFY_WITH_TIMEOUT(info->height() > 0, 5000);
    // Inside the small bold caption: drag end to end of the
    // caption ("Meanings: " plus one space). The end is probed,
    // not assumed: small/bold run styling and font metrics move
    // it around. Selecting through the caption must not reach the
    // value text ("eat").
    const QPoint from(4, 9);
    QVERIFY(info->characterAt(from) >= 0);
    QTest::mousePress(info, Qt::LeftButton, Qt::NoModifier, from);
    QPoint to = from;
    while (to.x() < info->width()) {
        const int at = info->characterAt(to + QPoint(1, 0));
        if (at < 0 || at >= static_cast<int>(QStringLiteral("Meanings: ").size())) {
            break;
        }
        to += QPoint(1, 0);
    }
    QVERIFY(info->characterAt(to) >= 0);
    QTest::mouseMove(info, to);
    QTest::mouseRelease(info, Qt::LeftButton, Qt::NoModifier, to);
    const QString picked = info->selectedText();
    QVERIFY(!picked.isEmpty());
    QVERIFY(picked.contains(QStringLiteral("eanings")));
    QVERIFY(!picked.contains(QStringLiteral("eat")));
}

void TestMainWindow::kanjiStrokeBadgesAvoidOverlap() {
    // Two near-identical vertical starts (strokes 1 and 5 of 我
    // collide the same way): the strict rule would stack both
    // badges, the placer separates them.
    KanjiStrokeView view;
    view.resize(260, 300);
    view.setStrokes({{{10, 10}, {10, 200}}, {{12, 12}, {12, 200}}});
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    const QVector<QRectF> badges = view.badgeRects();
    QCOMPARE(badges.size(), 2);
    QVERIFY(!badges[0].isNull() && !badges[1].isNull());
    QVERIFY(!badges[0].intersects(badges[1]));
}

void TestMainWindow::kanjiStrokeViewPaints() {
    // Hermetic paint proof: strokes + badges change pixels over the
    // empty view (theme-independent — compares grabs, not colors).
    KanjiStrokeView view;
    view.resize(260, 300);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    const QImage bare = view.grab().toImage();
    view.setStrokes({{{10, 10}, {200, 10}}, {{10, 10}, {10, 200}}});
    QCOMPARE(view.strokeCount(), 2);
    QTest::qWait(100);
    const QImage drawn = view.grab().toImage();
    QCOMPARE(bare.size(), drawn.size());
    int changed = 0;
    int vivid = 0;
    for (int y = 0; y < bare.height(); ++y) {
        for (int x = 0; x < bare.width(); ++x) {
            if (bare.pixel(x, y) != drawn.pixel(x, y)) {
                ++changed;
            }
            // Badge fills ramp saturated blue → green (strokes,
            // grid, and ground are all near-gray).
            if (drawn.pixelColor(x, y).hsvSaturation() > 100) {
                ++vivid;
            }
        }
    }
    QVERIFY2(changed > 200, "stroke diagram painted nothing");
    QVERIFY2(vivid > 20, "stroke badges painted nothing");
}

void TestMainWindow::candidateDoubleClickOpensDialog() {
    // Fast double-click on one candidate: single insert plus the
    // dialog (not a double insert).
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    QTest::mouseClick(openButtonOf(window.get()), Qt::LeftButton);
    auto *list = panelOf(window.get())->findChild<QListWidget *>("candidatesList");
    QVERIFY2(list != nullptr, "candidates list missing");
    list->addItem(QStringLiteral("食"));
    QApplication::processEvents();
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    input->clear();
    const QPoint at = list->visualRect(list->model()->index(0, 0)).center();
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, at);
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, at);
    QCOMPARE(input->text(), QStringLiteral("食"));
    auto *dialog = window->findChild<KanjiDialog *>("kanjiDialog");
    QVERIFY2(dialog != nullptr, "kanji dialog missing");
    QTRY_VERIFY_WITH_TIMEOUT(dialog->isVisible(), 5000);
    QCOMPARE(dialog->character(), QStringLiteral("食"));
}

void TestMainWindow::candidateSlowClicksInsertTwice() {
    // Same item clicked apart (past the double-click interval):
    // two inserts, no dialog — the timing, not the item, decides.
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    QTest::mouseClick(openButtonOf(window.get()), Qt::LeftButton);
    auto *list = panelOf(window.get())->findChild<QListWidget *>("candidatesList");
    QVERIFY2(list != nullptr, "candidates list missing");
    list->addItem(QStringLiteral("食"));
    QApplication::processEvents();
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    input->clear();
    const QPoint at = list->visualRect(list->model()->index(0, 0)).center();
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, at);
    QTest::qWait(700);
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, at);
    QCOMPARE(input->text(), QStringLiteral("食食"));
    QVERIFY(window->findChild<KanjiDialog *>("kanjiDialog") == nullptr);
}

void TestMainWindow::kanjiInfoStrokesFromData() {
    const QString path = kanjiArtifact();
    QVERIFY2(!path.isEmpty(), "no kanji.bin under ./ ../ ../../ ../../../data");
    KanjiInfo info;
    QVERIFY(info.load(path));
    const auto strokes = info.strokes(QStringLiteral("五"));
    QCOMPARE(strokes.size(), 4);
    for (const auto &stroke : strokes) {
        QVERIFY(!stroke.isEmpty());
        for (const QPointF &p : stroke) {
            QVERIFY(p.x() >= 0.0 && p.x() <= 255.0);
            QVERIFY(p.y() >= 0.0 && p.y() <= 255.0);
        }
    }
    // Non-Han and multi-char inputs carry no geometry.
    QVERIFY(info.strokes(QStringLiteral("あ")).isEmpty());
    QVERIFY(info.strokes(QStringLiteral("五五")).isEmpty());
}

void TestMainWindow::kanjiStrokePlaybackSteps() {
    // Hermetic stepping: deterministic, no timers involved.
    KanjiStrokeView view;
    view.setStrokes({{{10, 10}, {200, 10}}, {{10, 10}, {10, 200}}, {{200, 10}, {200, 200}}});
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    // New strokes land fully shown (the static diagram state).
    QCOMPARE(view.shownCount(), 3);
    view.stepBack();
    QCOMPARE(view.shownCount(), 2);
    view.stepBack();
    QCOMPARE(view.shownCount(), 1);
    view.stepBack();
    QCOMPARE(view.shownCount(), 0);
    view.stepBack();
    QCOMPARE(view.shownCount(), 0);
    view.stepForward();
    QCOMPARE(view.shownCount(), 1);
    view.pause();
    QCOMPARE(view.shownCount(), 1);
}

void TestMainWindow::kanjiStrokePlaybackPlays() {
    // Timer playback advances to full, then stops by itself.
    KanjiStrokeView view;
    view.setStrokes({{{10, 10}, {200, 10}}, {{10, 10}, {10, 200}}});
    view.stepBack();
    QCOMPARE(view.shownCount(), 1);
    view.setSpeed(8);
    view.play();
    QTRY_VERIFY_WITH_TIMEOUT(view.shownCount() == 2, 5000);
    view.pause();
    QCOMPARE(view.shownCount(), 2);
}

void TestMainWindow::kanjiDialogShowsCounter() {
    // Counter tracks the diagram through real data (04 for 五).
    const QString path = kanjiArtifact();
    QVERIFY2(!path.isEmpty(), "no kanji.bin under ./ ../ ../../ ../../../data");
    KanjiInfo info;
    QVERIFY(info.load(path));
    KanjiDialog dialog(&info, nullptr);
    dialog.setCharacter(QStringLiteral("五"));
    auto *counter = dialog.findChild<QLabel *>("kanjiStrokeCounter");
    QVERIFY2(counter != nullptr, "stroke counter missing");
    QCOMPARE(counter->text(), QStringLiteral("04"));
}

void TestMainWindow::kanjiDialogWordsTab() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    // Open the dialog, then switch to Words (lazy fill per tab).
    const QRect cell = table->visualRect(model->index(0, 0));
    QVERIFY(!cell.isNull());
    QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                       cell.topLeft() + QPoint(6, cell.height() / 2));
    auto *dialog = window->findChild<KanjiDialog *>("kanjiDialog");
    QVERIFY2(dialog != nullptr, "kanji dialog missing");
    QTRY_VERIFY_WITH_TIMEOUT(dialog->isVisible(), 5000);
    auto *tabs = dialog->findChild<QTabWidget *>("kanjiTabs");
    QVERIFY2(tabs != nullptr, "dialog tabs missing");
    // Exactly the Info + Words tabs, in that order.
    QCOMPARE(tabs->count(), 2);
    QCOMPARE(tabs->tabText(0), QStringLiteral("Info"));
    QCOMPARE(tabs->tabText(1), QStringLiteral("Words"));
    auto *words = dialog->findChild<DictionaryTable *>("kanjiWordsTable");
    QVERIFY2(words != nullptr, "words table missing");
    QCOMPARE(words->model()->rowCount(), 0);
    tabs->setCurrentIndex(1);
    QTRY_VERIFY_WITH_TIMEOUT(words->model()->rowCount() > 0, 10000);
    // Click a row whose heading holds the character: it searches
    // (same path as article links).
    int picked = -1;
    for (int row = 0; row < words->model()->rowCount(); ++row) {
        if (words->model()->index(row, 0).data(Qt::DisplayRole).toString().contains(QChar(u'食'))) {
            picked = row;
            break;
        }
    }
    QVERIFY2(picked >= 0, "no row holding 食");
    const QString heading = words->model()->index(picked, 0).data(Qt::DisplayRole).toString();
    QTest::mouseClick(words->viewport(), Qt::LeftButton, Qt::NoModifier,
                      words->visualRect(words->model()->index(picked, 0)).center());
    QCOMPARE(input->text(), heading);
}

void TestMainWindow::kanjiDialogSimilarStrip() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    const QRect cell = table->visualRect(model->index(0, 0));
    QVERIFY(!cell.isNull());
    QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                       cell.topLeft() + QPoint(6, cell.height() / 2));
    auto *dialog = window->findChild<KanjiDialog *>("kanjiDialog");
    QVERIFY2(dialog != nullptr, "kanji dialog missing");
    QTRY_VERIFY_WITH_TIMEOUT(dialog->isVisible(), 5000);
    // Similar is a bottom checkbox row with a candidate strip
    // (not a tab): unchecked hides the strip, checking fills it
    // with single kanji.
    auto *group = dialog->findChild<QCheckBox *>("kanjiSimilarCheck");
    QVERIFY2(group != nullptr, "similar checkbox missing");
    auto *strip = dialog->findChild<CandidateList *>("kanjiSimilarStrip");
    QVERIFY2(strip != nullptr, "similar strip missing");
    QVERIFY(!strip->isVisible());
    group->setChecked(true);
    QTRY_VERIFY_WITH_TIMEOUT(strip->count() > 0, 15000);
    for (int i = 0; i < strip->count(); ++i) {
        QCOMPARE(strip->item(i)->text().size(), 1);
        QVERIFY(strip->item(i)->text() != QStringLiteral("食"));
    }
    // Tapping a similar shape retargets the whole dialog.
    const QString target = strip->item(0)->text();
    QTest::mouseClick(strip->viewport(), Qt::LeftButton, Qt::NoModifier,
                      strip->visualRect(strip->model()->index(0, 0)).center());
    QTRY_VERIFY_WITH_TIMEOUT(dialog->character() == target, 5000);
    group->setChecked(false);
    QVERIFY(!strip->isVisible());
}

void TestMainWindow::kanjiDialogPartsStrip() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    input->setText(QStringLiteral("亜鉛"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    const QRect cell = table->visualRect(model->index(0, 0));
    QVERIFY(!cell.isNull());
    QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                       cell.topLeft() + QPoint(6, cell.height() / 2));
    auto *dialog = window->findChild<KanjiDialog *>("kanjiDialog");
    QVERIFY2(dialog != nullptr, "kanji dialog missing");
    QTRY_VERIFY_WITH_TIMEOUT(dialog->isVisible(), 5000);
    QCOMPARE(dialog->character(), QStringLiteral("亜"));
    auto *group = dialog->findChild<QCheckBox *>("kanjiPartsCheck");
    QVERIFY2(group != nullptr, "parts checkbox missing");
    auto *strip = dialog->findChild<CandidateList *>("kanjiPartsStrip");
    QVERIFY2(strip != nullptr, "parts strip missing");
    group->setChecked(true);
    QTRY_VERIFY_WITH_TIMEOUT(strip->count() > 0, 5000);
    QStringList shown;
    for (int i = 0; i < strip->count(); ++i) {
        shown << strip->item(i)->text();
    }
    QVERIFY(shown.contains(QStringLiteral("｜")));
    QVERIFY(shown.contains(QStringLiteral("一")));
    QVERIFY(shown.contains(QStringLiteral("口")));
    // Drilling into a part retargets the dialog.
    int ichi = -1;
    for (int i = 0; i < strip->count(); ++i) {
        if (strip->item(i)->text() == QStringLiteral("一")) {
            ichi = i;
        }
    }
    QVERIFY2(ichi >= 0, "一 part missing");
    QTest::mouseClick(strip->viewport(), Qt::LeftButton, Qt::NoModifier,
                      strip->visualRect(strip->model()->index(ichi, 0)).center());
    QTRY_VERIFY_WITH_TIMEOUT(dialog->character() == QStringLiteral("一"), 5000);
    // Persisted open state: back to unchecked for suite isolation.
    group->setChecked(false);
}

void TestMainWindow::kanjiDialogPartOfStrip() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    input->setText(QStringLiteral("五"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    const QRect cell = table->visualRect(model->index(0, 0));
    QVERIFY(!cell.isNull());
    QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                       cell.topLeft() + QPoint(6, cell.height() / 2));
    auto *dialog = window->findChild<KanjiDialog *>("kanjiDialog");
    QVERIFY2(dialog != nullptr, "kanji dialog missing");
    QTRY_VERIFY_WITH_TIMEOUT(dialog->isVisible(), 5000);
    auto *group = dialog->findChild<QCheckBox *>("kanjiPartOfCheck");
    QVERIFY2(group != nullptr, "part-of checkbox missing");
    auto *strip = dialog->findChild<CandidateList *>("kanjiPartOfStrip");
    QVERIFY2(strip != nullptr, "part-of strip missing");
    group->setChecked(true);
    QTRY_VERIFY_WITH_TIMEOUT(strip->count() > 0, 5000);
    int go = -1;
    for (int i = 0; i < strip->count(); ++i) {
        if (strip->item(i)->text() == QStringLiteral("吾")) {
            go = i;
        }
    }
    QVERIFY2(go >= 0, "吾 missing from part-of");
    QTest::mouseClick(strip->viewport(), Qt::LeftButton, Qt::NoModifier,
                      strip->visualRect(strip->model()->index(go, 0)).center());
    QTRY_VERIFY_WITH_TIMEOUT(dialog->character() == QStringLiteral("吾"), 5000);
    // Persisted open state: back to unchecked for suite isolation.
    group->setChecked(false);
}

void TestMainWindow::kanjiDialogStripShowsKanjiTip() {
    // Dialog strips carry the kanji hover card (same card as the
    // table): hovering a similar shape shows its glyph.
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    const QRect cell = table->visualRect(model->index(0, 0));
    QVERIFY(!cell.isNull());
    QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                       cell.topLeft() + QPoint(6, cell.height() / 2));
    auto *dialog = window->findChild<KanjiDialog *>("kanjiDialog");
    QVERIFY2(dialog != nullptr, "kanji dialog missing");
    QTRY_VERIFY_WITH_TIMEOUT(dialog->isVisible(), 5000);
    auto *group = dialog->findChild<QCheckBox *>("kanjiSimilarCheck");
    QVERIFY2(group != nullptr, "similar checkbox missing");
    auto *strip = dialog->findChild<CandidateList *>("kanjiSimilarStrip");
    QVERIFY2(strip != nullptr, "similar strip missing");
    group->setChecked(true);
    QTRY_VERIFY_WITH_TIMEOUT(strip->count() > 0, 15000);
    const QString first = strip->item(0)->text();
    QCOMPARE(first.size(), 1);
    // Hand-built move (deterministic, no buttons involved). Park
    // the real cursor on the cell first: the double-click above
    // parks it over the main table, and its delayed motion events
    // would otherwise race this synthetic hover on the one shared
    // dialog card.
    const QPoint at = strip->visualRect(strip->model()->index(0, 0)).center();
    QCursor::setPos(strip->viewport()->mapToGlobal(at));
    QMouseEvent hover(QEvent::MouseMove, QPointF(at), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(strip->viewport(), &hover);
    auto *tip = dialog->findChild<QWidget *>("kanjiTip");
    QVERIFY2(tip != nullptr, "strip kanji tip missing");
    QTRY_VERIFY_WITH_TIMEOUT(tip->isVisible(), 5000);
    auto *glyph = tip->findChild<QLabel *>("kanjiGlyph");
    QVERIFY2(glyph != nullptr, "tip glyph missing");
    QCOMPARE(glyph->text(), first);
    group->setChecked(false);
}

void TestMainWindow::kanjiDialogTogglePreservesMainArea() {
    // Checking a heading resizes the window, never the tab area.
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    const QRect cell = table->visualRect(model->index(0, 0));
    QVERIFY(!cell.isNull());
    QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                       cell.topLeft() + QPoint(6, cell.height() / 2));
    auto *dialog = window->findChild<KanjiDialog *>("kanjiDialog");
    QVERIFY2(dialog != nullptr, "kanji dialog missing");
    QTRY_VERIFY_WITH_TIMEOUT(dialog->isVisible(), 5000);
    auto *tabs = dialog->findChild<QTabWidget *>("kanjiTabs");
    QVERIFY2(tabs != nullptr, "dialog tabs missing");
    auto *group = dialog->findChild<QCheckBox *>("kanjiSimilarCheck");
    QVERIFY2(group != nullptr, "similar checkbox missing");
    auto *strip = dialog->findChild<CandidateList *>("kanjiSimilarStrip");
    QVERIFY2(strip != nullptr, "similar strip missing");
    // Prefetched content lands without checking.
    QTRY_VERIFY_WITH_TIMEOUT(strip->count() > 0, 15000);
    group->setChecked(false);
    const int tabsHeight = tabs->height();
    const int dialogHeight = dialog->height();
    QVERIFY(tabsHeight > 0 && dialogHeight > 0);
    group->setChecked(true);
    QTRY_VERIFY_WITH_TIMEOUT(strip->isVisible(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(tabs->height() == tabsHeight && dialog->height() != dialogHeight,
                             5000);
    QCOMPARE(tabs->height(), tabsHeight);
    QVERIFY(dialog->height() > dialogHeight);
    group->setChecked(false);
    QTRY_VERIFY_WITH_TIMEOUT(!strip->isVisible(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(tabs->height() == tabsHeight && dialog->height() == dialogHeight,
                             5000);
    QCOMPARE(tabs->height(), tabsHeight);
    QCOMPARE(dialog->height(), dialogHeight);
}

void TestMainWindow::kanjiStrokePlaybackDrawsPartial() {
    // In-progress strokes interpolate by arc length: mid-animation
    // paint sits strictly between empty and full (no timers —
    // advances are invoked synchronously).
    KanjiStrokeView view;
    view.resize(260, 300);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    const QImage bare = view.grab().toImage();
    view.setStrokes({{{10, 10}, {200, 10}}, {{10, 10}, {10, 200}}});
    QTest::qWait(100);
    const QImage full = view.grab().toImage();
    const auto changedFrom = [&](const QImage &shot) {
        int changed = 0;
        for (int y = 0; y < bare.height(); ++y) {
            for (int x = 0; x < bare.width(); ++x) {
                if (bare.pixel(x, y) != shot.pixel(x, y)) {
                    ++changed;
                }
            }
        }
        return changed;
    };
    const int fullChanged = changedFrom(full);
    QVERIFY(fullChanged > 200);
    view.stepBack();
    view.stepBack();
    QCOMPARE(view.shownCount(), 0);
    for (int i = 0; i < 3; ++i) {
        QVERIFY(QMetaObject::invokeMethod(&view, "advance"));
    }
    QTest::qWait(100);
    const int midChanged = changedFrom(view.grab().toImage());
    QVERIFY2(midChanged > 50, "partial stroke painted nothing");
    QVERIFY2(midChanged < fullChanged, "partial stroke painted everything");
    view.pause();
}

void TestMainWindow::kanjiDialogStripStatePersists() {
    // Checkbox open state persists across dialog instances (same
    // memorized YAML as the panel size): flip, rebuild, restored.
    // Suite state is shared — all three keys reset on the way out.
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    const QRect cell = table->visualRect(model->index(0, 0));
    QVERIFY(!cell.isNull());
    QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                       cell.topLeft() + QPoint(6, cell.height() / 2));
    auto *dialog = window->findChild<KanjiDialog *>("kanjiDialog");
    QVERIFY2(dialog != nullptr, "kanji dialog missing");
    QTRY_VERIFY_WITH_TIMEOUT(dialog->isVisible(), 5000);
    auto *check = dialog->findChild<QCheckBox *>("kanjiSimilarCheck");
    QVERIFY2(check != nullptr, "similar checkbox missing");
    check->setChecked(false);
    check->setChecked(true);
    // Value landed in the memorized file.
    QCOMPARE(MemorizedState::kanjiStripOpen(MemorizedState::KanjiStrip::Similar), true);
    // A fresh dialog restores it (as across restarts).
    auto *widget = window->findChild<DictionaryWidget *>();
    QVERIFY2(widget != nullptr, "dictionary widget missing");
    auto *dictModel = qobject_cast<DictionaryModel *>(model);
    QVERIFY2(dictModel != nullptr, "main model missing");
    KanjiDialog fresh(widget->kanjiData(), dictModel, window.get());
    auto *freshCheck = fresh.findChild<QCheckBox *>("kanjiSimilarCheck");
    QVERIFY2(freshCheck != nullptr, "fresh similar checkbox missing");
    QVERIFY(freshCheck->isChecked());
    // Suite isolation: back to unchecked.
    check->setChecked(false);
    QCOMPARE(MemorizedState::kanjiStripOpen(MemorizedState::KanjiStrip::Similar), false);
}

void TestMainWindow::kanjiDialogRetargetShowsInfo() {
    // Retargeting the dialog resets it to the Info tab (the Words
    // tab belongs to the old character until the user switches).
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    const QRect cell = table->visualRect(model->index(0, 0));
    QVERIFY(!cell.isNull());
    QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                       cell.topLeft() + QPoint(6, cell.height() / 2));
    auto *dialog = window->findChild<KanjiDialog *>("kanjiDialog");
    QVERIFY2(dialog != nullptr, "kanji dialog missing");
    QTRY_VERIFY_WITH_TIMEOUT(dialog->isVisible(), 5000);
    auto *tabs = dialog->findChild<QTabWidget *>("kanjiTabs");
    QVERIFY2(tabs != nullptr, "dialog tabs missing");
    tabs->setCurrentIndex(1);
    QCOMPARE(tabs->currentIndex(), 1);
    dialog->setCharacter(QStringLiteral("日"));
    QCOMPARE(dialog->character(), QStringLiteral("日"));
    QCOMPARE(tabs->currentIndex(), 0);
}
