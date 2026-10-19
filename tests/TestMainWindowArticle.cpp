#include "TestMainWindow.h"

#include <QtTest>

#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

#include "core/dictionary/DictionaryModel.h"
#include "core/palette/MaterialPalette.h"
#include "ui/mainwindow/MainWindow.h"
#include "widgets/table/DictionaryTable.h"
#include "widgets/text/ArticleView.h"
#include "widgets/text/RubyLabel.h"
#include "widgets/text/RunLabel.h"

#include <memory>

void TestMainWindow::dictionaryArticleFollowsSelection() {
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
    // No selection yet: pane stays hidden, table owns the width.
    QVERIFY(!article->isVisible());
    // Clicking a row shows the full article (heading, numbered
    // senses, tags) next to the table.
    const QRect cell = table->visualRect(model->index(0, 0));
    QVERIFY(!cell.isNull());
    QTest::mouseClick(table->viewport(), Qt::LeftButton, {}, cell.center());
    QTRY_VERIFY_WITH_TIMEOUT(article->isVisible(), 5000);
    auto *heading = article->findChild<RubyLabel *>(QStringLiteral("articleHeading"));
    QVERIFY2(heading != nullptr, "article heading missing");
    QCOMPARE(heading->text(), QStringLiteral("食べる"));
    const QList<RunLabel *> senses =
        article->findChildren<RunLabel *>(QStringLiteral("articleSense"));
    QVERIFY(!senses.isEmpty());
    QVERIFY(senses.first()->text().startsWith(QStringLiteral("1.")));
    QVERIFY(senses.first()->text().contains(QStringLiteral("vt")));
    // Jisho-style full article: common badge, remaining forms with
    // annotations, all tag categories on their senses.
    auto *common = article->findChild<QLabel *>(QStringLiteral("commonBadge"));
    QVERIFY2(common != nullptr, "common badge missing");
    QCOMPARE(common->text(), QStringLiteral("Common word"));
    // JLPT badge rides the tanos snapshot (たべる is N5).
    auto *jlpt = article->findChild<QLabel *>(QStringLiteral("jlptBadge"));
    QVERIFY2(jlpt != nullptr, "JLPT badge missing");
    QCOMPARE(jlpt->text(), QStringLiteral("JLPT N5"));
    bool foundForm = false;
    const QList<RubyLabel *> forms =
        article->findChildren<RubyLabel *>(QStringLiteral("articleForm"));
    for (const RubyLabel *form : forms) {
        foundForm = foundForm || form->text().contains(QStringLiteral("喰べる"));
    }
    QVERIFY2(foundForm, "other-forms heading missing");
    // Second row (食べるラー油) carries a field tag on sense 1.
    const QRect cell2 = table->visualRect(model->index(1, 0));
    QVERIFY(!cell2.isNull());
    QTest::mouseClick(table->viewport(), Qt::LeftButton, {}, cell2.center());
    bool foundField = false;
    QTRY_VERIFY_WITH_TIMEOUT(
        [&] {
            foundField = false;
            const QList<RunLabel *> rows =
                article->findChildren<RunLabel *>(QStringLiteral("articleSense"));
            for (const RunLabel *sense : rows) {
                foundField = foundField || sense->text().contains(QStringLiteral("food"));
            }
            return foundField;
        }(),
        5000);
    QVERIFY(foundField);
    // Clearing the search drops the rows and the pane with them.
    input->setText(QString());
    QTRY_VERIFY_WITH_TIMEOUT(!article->isVisible(), 5000);
}

void TestMainWindow::dictionaryArticleLinkInsertsQuery() {
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
    input->setText(QStringLiteral("いく"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    // First row whose article carries meta references (See also,
    // antonyms, restricted forms).
    const int scanned = qMin(model->rowCount(), 10);
    int linkedRow = -1;
    const auto hasMeta = [&] {
        const QList<RunLabel *> senses =
            article->findChildren<RunLabel *>(QStringLiteral("articleSense"));
        for (const RunLabel *sense : senses) {
            const QString text = sense->text();
            if (text.contains(QStringLiteral("See also")) ||
                text.contains(QStringLiteral("Antonyms")) ||
                text.contains(QStringLiteral("Only:"))) {
                return true;
            }
        }
        return false;
    };
    for (int row = 0; row < scanned; ++row) {
        table->selectRow(row);
        if (hasMeta()) {
            linkedRow = row;
            break;
        }
    }
    QVERIFY2(linkedRow >= 0, "no meta-reference article in first rows");
    // References sit below the senses: take the first visible
    // link hit and click it as a user would.
    const QList<ArticleView::LinkHit> links = article->linkHits();
    const QRect visible = article->viewport()->rect();
    ArticleView::LinkHit hit;
    for (const ArticleView::LinkHit &candidate : links) {
        if (visible.contains(candidate.rect.center())) {
            hit = candidate;
            break;
        }
    }
    QVERIFY2(hit.label != nullptr, "no visible article link found");
    const QString query = hit.href;
    QVERIFY(!query.isEmpty());
    QVERIFY(!query.contains(u'・'));
    QTest::mouseClick(hit.label, Qt::LeftButton, Qt::NoModifier,
                      hit.label->mapFrom(article->viewport(), hit.rect.center()));
    // The headword lands in the search field (the `・1` reference
    // tail is display-only) and the lookup runs.
    QCOMPARE(input->text(), query);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
}

void TestMainWindow::dictionaryArticleRealXrefRendersAsLink() {
    // 足 ("money" sense) cross-references お足: the term must
    // carry its query and render as a link (blue + underlined),
    // not dim text.
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
    input->setText(QStringLiteral("足"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    int ashiRow = -1;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (model->index(row, 0).data(Qt::DisplayRole).toString() == QStringLiteral("足")) {
            ashiRow = row;
            break;
        }
    }
    QVERIFY2(ashiRow >= 0, "足 row missing");
    table->selectRow(ashiRow);
    QTRY_VERIFY_WITH_TIMEOUT(article->isVisible(), 5000);
    // The xref term carries its headword query.
    RunLabel *host = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(([&] {
                                 for (RunLabel *sense : article->findChildren<RunLabel *>(
                                          QStringLiteral("articleSense"))) {
                                     for (const auto &[rect, href] : sense->links()) {
                                         if (href == QStringLiteral("お足")) {
                                             host = sense;
                                             return true;
                                         }
                                     }
                                 }
                                 return false;
                             })(),
                             5000);
    QVERIFY2(host != nullptr, "お足 xref carries no query");
    // ... and renders as a link (blue + underlined).
    const MaterialPalette theme = MaterialPalette::fromPalette(host->palette());
    bool sawStyled = false;
    for (int p = 0; p < 40; ++p) {
        for (const QTextLayout::FormatRange &range : host->formatsForParagraph(p)) {
            if (range.format.fontUnderline()) {
                QCOMPARE(range.format.foreground().color(), theme.link());
                sawStyled = true;
            }
        }
    }
    QVERIFY2(sawStyled, "お足 renders without link styling");
}

void TestMainWindow::dictionaryArticleShowsTextCursor() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *article = window->findChild<ArticleView *>("articleView");
    QVERIFY2(article != nullptr, "article pane missing");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(table->model()->rowCount() > 0, 30000);
    table->selectRow(0);
    QTRY_VERIFY_WITH_TIMEOUT(article->isVisible(), 5000);
    // I-beam over article text (labels carry it; the viewport keeps
    // it for gaps).
    QCOMPARE(article->viewport()->cursor().shape(), Qt::IBeamCursor);
    auto *headingCursor = article->findChild<RubyLabel *>(QStringLiteral("articleHeading"));
    QVERIFY2(headingCursor != nullptr, "article heading missing");
    QCOMPARE(headingCursor->cursor().shape(), Qt::IBeamCursor);
}

void TestMainWindow::dictionaryArticleCloseButtonHidesPane() {
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
    QVERIFY(table->selectionModel()->hasSelection());
    // The dismiss chip rides on the article pane itself.
    auto *close = article->findChild<QToolButton *>(QStringLiteral("articleCloseButton"));
    QVERIFY2(close != nullptr, "article close button missing");
    QVERIFY(close->isVisible());
    QTest::mouseClick(close, Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(!article->isVisible(), 5000);
    QVERIFY(!table->selectionModel()->hasSelection());
    QVERIFY(!table->currentIndex().isValid());
}

void TestMainWindow::dictionaryArticleSpanSelectsMeanings() {
    // Press-drag from one sense block into the next selects across
    // both (meanings only); one clipboard write covers the span.
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
    // First row whose article holds at least two sense blocks.
    QList<RunLabel *> senses;
    const int scanned = qMin(model->rowCount(), 10);
    for (int row = 0; row < scanned && senses.size() < 2; ++row) {
        table->selectRow(row);
        QTRY_VERIFY_WITH_TIMEOUT(article->isVisible(), 5000);
        senses = article->findChildren<RunLabel *>(QStringLiteral("articleSense"));
    }
    QVERIFY2(senses.size() >= 2, "no two-sense article in first rows");
    RunLabel *first = senses.at(0);
    RunLabel *second = senses.at(1);
    QTRY_VERIFY_WITH_TIMEOUT(first->height() > 0 && second->height() > 0, 5000);
    const QPoint from(10, first->height() / 2);
    QVERIFY(first->characterAt(from) >= 0);
    QTest::mousePress(first, Qt::LeftButton, Qt::NoModifier, from);
    const QPoint to(10, second->height() / 2);
    QVERIFY(second->characterAt(to) >= 0);
    QTest::mouseMove(second, to);
    QTest::mouseRelease(second, Qt::LeftButton, Qt::NoModifier, to);
    // Both blocks contribute, joined as paragraphs.
    const QString joined = first->selectedText() + QStringLiteral("\n") + second->selectedText();
    QVERIFY(!first->selectedText().isEmpty());
    QVERIFY(!second->selectedText().isEmpty());
    QCOMPARE(article->selectedText(), joined);
    // Each span block is a contiguous slice of its label.
    const QStringList blocks = joined.split(QStringLiteral("\n"));
    QCOMPARE(blocks.size(), 2);
    QVERIFY(first->text().contains(blocks.at(0)));
    QVERIFY(second->text().contains(blocks.at(1)));
    // One copy for the whole span (the focused label's own copy
    // would only cover its block).
    QTest::keyClick(first, Qt::Key_C, Qt::ControlModifier);
    QTRY_VERIFY_WITH_TIMEOUT(QApplication::clipboard()->text() == joined, 5000);
}

void TestMainWindow::dictionaryArticleSpanClearsBeyondFocus() {
    // Dragging through the inter-label gaps must not pre-select
    // every meaning: only passed senses fill, and landing on a
    // sense clears whatever gap transit filled past it.
    // Synthetic article (no dictionary data), three senses.
    ArticleView view;
    ArticleDisplay article;
    article.heading = QStringLiteral("食べる");
    article.reading = QStringLiteral("たべる");
    SenseDisplay one;
    one.gloss = QStringLiteral("to eat");
    SenseDisplay two;
    two.gloss = QStringLiteral("to live");
    SenseDisplay three;
    three.gloss = QStringLiteral("to be");
    article.senses = {one, two, three};
    view.setArticle(article);
    view.resize(300, 400);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view, 5000));
    const QList<RunLabel *> senses = view.findChildren<RunLabel *>(QStringLiteral("articleSense"));
    QCOMPARE(senses.size(), 3);
    RunLabel *first = senses.at(0);
    RunLabel *second = senses.at(1);
    RunLabel *third = senses.at(2);
    QTRY_VERIFY_WITH_TIMEOUT(first->height() > 0 && third->height() > 0, 5000);
    QWidget *viewport = view.viewport();
    const QPoint pressAt(10, first->height() / 2);
    QVERIFY(first->characterAt(pressAt) >= 0);
    // Entry point into the second sense: past char 0 on any font
    // (x=10 can sit inside a wide first glyph, leaving a (0,0)
    // empty range and failing the focus checks below).
    QPoint secondAt(10, second->height() / 2);
    while (secondAt.x() < second->width() - 6 && second->characterAt(secondAt) < 2) {
        secondAt.rx() += 4;
    }
    QVERIFY2(second->characterAt(secondAt) >= 2, "no mid-word point in second sense");
    // Gap between the first two senses (no label under it):
    // the midpoint is maximally clear of both on any font.
    const int firstBottom = first->mapTo(viewport, QPoint(0, first->height())).y();
    const int secondTop = second->mapTo(viewport, QPoint(0, 0)).y();
    QVERIFY2(secondTop > firstBottom, "no gap between first two senses");
    const QPoint gapAt(5, (firstBottom + secondTop) / 2);
    QVERIFY2(view.labelAt(gapAt, nullptr) == nullptr, "gap point hits a label");
    QTest::mousePress(first, Qt::LeftButton, Qt::NoModifier, pressAt);
    // Through the gap: the unreached senses stay clear.
    QTest::mouseMove(viewport, gapAt);
    QVERIFY2(!first->selectedText().isEmpty(), "gap move cleared the anchor");
    QVERIFY2(second->selectedText().isEmpty(), "gap transit filled the unreached sense");
    QVERIFY2(third->selectedText().isEmpty(), "gap transit filled the trailing sense");
    // Into the second sense: stale fills past the focus clear.
    QTest::mouseMove(second, secondAt);
    QVERIFY2(!first->selectedText().isEmpty(), "second move cleared the anchor");
    QVERIFY2(!second->selectedText().isEmpty(), "second move left the focus empty");
    QVERIFY2(second->text().startsWith(second->selectedText()), "focus fill not anchored at start");
    QVERIFY2(third->selectedText().isEmpty(), "trailing sense kept its gap fill");
    // Overshoot past the last sense still keeps the full span.
    const QPoint pastEnd = third->mapTo(viewport, QPoint(5, third->height() + 20));
    QTest::mouseMove(viewport, pastEnd);
    QVERIFY2(!first->selectedText().isEmpty(), "overshoot cleared the anchor");
    QVERIFY2(!second->selectedText().isEmpty(), "overshoot cleared the middle");
    QVERIFY2(!third->selectedText().isEmpty(), "overshoot left the tail empty");
    // Back into the second sense: the overshoot fill past the
    // focus clears.
    QTest::mouseMove(second, secondAt);
    QVERIFY2(!first->selectedText().isEmpty(), "step-back cleared the anchor");
    QVERIFY2(!second->selectedText().isEmpty(), "step-back left the focus empty");
    QVERIFY2(third->selectedText().isEmpty(), "overshoot fill past the focus kept");
    QTest::mouseRelease(second, Qt::LeftButton, Qt::NoModifier, secondAt);
}

void TestMainWindow::dictionaryArticleSpanIgnoresHeading() {
    // A drag starting in the heading never spans: headings keep
    // their single-label selection, meanings stay untouched.
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
    const QList<RunLabel *> senses =
        article->findChildren<RunLabel *>(QStringLiteral("articleSense"));
    QVERIFY(!senses.isEmpty());
    RunLabel *sense = senses.first();
    QTRY_VERIFY_WITH_TIMEOUT(heading->height() > 0 && sense->height() > 0, 5000);
    QTest::mousePress(heading, Qt::LeftButton, Qt::NoModifier, QPoint(4, heading->height() / 2));
    QTest::mouseMove(sense, QPoint(10, sense->height() / 2));
    QTest::mouseRelease(sense, Qt::LeftButton, Qt::NoModifier, QPoint(10, sense->height() / 2));
    QVERIFY(article->selectedText().isEmpty());
    QVERIFY(sense->selectedText().isEmpty());
}
