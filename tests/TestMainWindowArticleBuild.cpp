#include "TestMainWindow.h"

#include <QtTest>

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QToolTip>

#include "core/dictionary/DictionaryModel.h"
#include "core/dictionary/MeaningBuilder.h"
#include "core/palette/MaterialPalette.h"
#include "core/settings/Settings.h"
#include "ui/mainwindow/MainWindow.h"
#include "util/Romaji.h"
#include "widgets/table/DictionaryTable.h"
#include "widgets/text/ArticleRuns.h"
#include "widgets/text/ArticleView.h"
#include "widgets/text/RubyLabel.h"
#include "widgets/text/RunLabel.h"

#include <QClipboard>
#include <memory>

void TestMainWindow::dictionaryArticleBuildsWidgets() {
    // Widget structure for a hand-built fixture (no data files):
    // every branch, no HTML anywhere. Same fixture shape as the
    // old byte-exact template test (escaping edges included).
    AppSettings::shared().setReading(ReadingDisplay::Kana);
    ArticleDisplay article;
    article.heading = QStringLiteral("A&B");
    article.reading = QStringLiteral("あい");
    article.jlpt = 5;
    ElementDisplay headForm;
    headForm.form = QStringLiteral("A&B");
    headForm.priorities = QStringList{QStringLiteral("ichi1")};
    ElementDisplay otherKanji;
    otherKanji.form = QStringLiteral("漢字");
    otherKanji.info = QStringList{QStringLiteral("outdated")};
    article.kanji = {headForm, otherKanji};
    ElementDisplay headReading;
    headReading.form = QStringLiteral("あい");
    ElementDisplay otherReading;
    otherReading.form = QStringLiteral("のみ");
    otherReading.noKanji = true;
    article.readings = {headReading, otherReading};
    SenseDisplay first;
    first.pos = QStringList{QStringLiteral("v1")};
    first.misc = QStringList{QStringLiteral("uk")};
    first.gloss = QStringLiteral("to eat & drink");
    first.field = QStringList{QStringLiteral("food")};
    first.xref = QStringList{QStringLiteral("飲む・1")};
    first.ant = QStringList{QStringLiteral("X")};
    first.onlyKeb = QStringList{QStringLiteral("Y")};
    first.notes = QStringList{QStringLiteral("note <x>")};
    SourceDisplay source;
    source.lang = QStringLiteral("fr");
    source.text = QStringLiteral("manger & boire");
    first.sources = {source};
    SenseDisplay second;
    second.gloss = QStringLiteral("kid's meal");
    article.senses = {first, second};

    ArticleView view;
    view.resize(400, 600);
    view.show();
    view.setArticle(article);
    // Heading (ruby over the whole form here), reading line,
    // badges with exact texts.
    auto *heading = view.findChild<RubyLabel *>(QStringLiteral("articleHeading"));
    QVERIFY2(heading != nullptr, "heading missing");
    QCOMPARE(heading->text(), QStringLiteral("A&B"));
    auto *reading = view.findChild<RunLabel *>(QStringLiteral("articleReading"));
    QVERIFY2(reading != nullptr, "reading line missing");
    QCOMPARE(reading->text(), QStringLiteral("あい"));
    auto *common = view.findChild<QLabel *>(QStringLiteral("commonBadge"));
    QVERIFY2(common != nullptr, "common badge missing");
    QCOMPARE(common->text(), QStringLiteral("Common word"));
    auto *jlpt = view.findChild<QLabel *>(QStringLiteral("jlptBadge"));
    QVERIFY2(jlpt != nullptr, "JLPT badge missing");
    QCOMPARE(jlpt->text(), QStringLiteral("JLPT N5"));
    // One run block per sense: number, tags, gloss, meta lines.
    const QList<RunLabel *> senses = view.findChildren<RunLabel *>(QStringLiteral("articleSense"));
    QCOMPARE(senses.size(), 2);
    QVERIFY(senses[0]->text().startsWith(QStringLiteral("1. ")));
    QVERIFY(senses[0]->text().contains(QStringLiteral("v1")));
    QVERIFY(senses[0]->text().contains(QStringLiteral("to eat & drink")));
    QVERIFY(senses[0]->text().contains(QStringLiteral("See also: ")));
    QVERIFY(senses[1]->text().startsWith(QStringLiteral("2. ")));
    // Meta links carry raw headword queries (the ・ tail is
    // display-only, like before). Rects need a paint pass, so poll.
    const auto hasLink = [&](const QString &query) {
        for (const ArticleView::LinkHit &hit : view.linkHits()) {
            if (hit.href == query) {
                return true;
            }
        }
        return false;
    };
    QTRY_VERIFY_WITH_TIMEOUT(hasLink(QStringLiteral("飲む")) && hasLink(QStringLiteral("X")) &&
                                 hasLink(QStringLiteral("Y")),
                             5000);
    for (const ArticleView::LinkHit &hit : view.linkHits()) {
        QVERIFY(!hit.href.contains(u'・'));
    }
    // Remaining forms (ruby heading + plain reading with notes).
    const QList<RubyLabel *> forms = view.findChildren<RubyLabel *>(QStringLiteral("articleForm"));
    QCOMPARE(forms.size(), 1);
    QCOMPARE(forms.first()->text(), QStringLiteral("漢字"));
    const QList<RunLabel *> plainForms =
        view.findChildren<RunLabel *>(QStringLiteral("articleForm"));
    QCOMPARE(plainForms.size(), 1);
    QCOMPARE(plainForms.first()->text(), QStringLiteral("のみ (no kanji)"));
}

void TestMainWindow::dictionaryArticleMetaLinesBreak() {
    // "\nSee also: " must start its own line (QTextLayout ignores
    // embedded newlines, so one layout glued it onto the gloss).
    AppSettings::shared().setReading(ReadingDisplay::Kana);
    ArticleDisplay article;
    article.heading = QStringLiteral("食べる");
    article.reading = QStringLiteral("たべる");
    SenseDisplay sense;
    sense.gloss = QStringLiteral("to eat");
    sense.xref = QStringList{QStringLiteral("食べ物")};
    article.senses = {sense};

    ArticleView view;
    view.resize(400, 600);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.setArticle(article);
    // Label-local link geometry: the xref must sit below the first
    // text line, not glued onto it. Poll for layout (width first:
    // a narrow label would wrap even glued text).
    auto *senseLabel = view.findChild<RunLabel *>(QStringLiteral("articleSense"));
    QVERIFY2(senseLabel != nullptr, "sense label missing");
    QTRY_VERIFY_WITH_TIMEOUT(senseLabel->width() > 300, 5000);
    QRect linkRect;
    QTRY_VERIFY_WITH_TIMEOUT(([&] {
                                 const auto links = senseLabel->links();
                                 if (links.isEmpty()) {
                                     return false;
                                 }
                                 linkRect = links.first().first;
                                 return true;
                             })(),
                             5000);
    QVERIFY2(linkRect.y() > 0, "xref glued onto the gloss line");
}

void TestMainWindow::dictionaryArticleLinksUnderlined() {
    // Href runs read as links: blue (Link role) and underlined,
    // like the old browser pane. Bare Link-ink runs (field tags)
    // stay flat.
    RunLabel label;
    label.setRuns(
        {{QStringLiteral("see "), RunLabel::Ink::Body},
         {QStringLiteral("field"), RunLabel::Ink::Link, true, false, {}},
         {QStringLiteral("target"), RunLabel::Ink::Link, true, false, QStringLiteral("query")}});
    label.resize(400, 100);
    label.show();
    QVERIFY(QTest::qWaitForWindowExposed(&label));
    const auto formats = label.formatsForParagraph(0);
    QVERIFY2(!formats.isEmpty(), "no format ranges");
    const MaterialPalette theme = MaterialPalette::fromPalette(label.palette());
    bool sawLink = false;
    for (const QTextLayout::FormatRange &range : formats) {
        if (range.start == 4 + 5 && range.length == 6) {
            // The href run: blue + underlined.
            QVERIFY(range.format.fontUnderline());
            QCOMPARE(range.format.foreground().color(), theme.link());
            sawLink = true;
        } else {
            QVERIFY(!range.format.fontUnderline());
        }
    }
    QVERIFY2(sawLink, "href run missing from formats");
}

void TestMainWindow::dictionaryArticleUkRendersRed() {
    // Article misc splits like the table: `uk` reads warning-red,
    // other tags stay dim.
    SenseDisplay sense;
    sense.gloss = QStringLiteral("to eat");
    sense.misc = QStringList{QStringLiteral("uk"), QStringLiteral("col")};
    const QVector<RunLabel::Run> runs = ArticleRuns::senseRuns(sense, 1);
    // Number, uk, col, gloss.
    QCOMPARE(runs.size(), 4);
    QCOMPARE(runs[1].text, QStringLiteral("kana, "));
    QCOMPARE(runs[1].ink, RunLabel::Ink::Red);
    QCOMPARE(runs[2].text, QStringLiteral("col "));
    QCOMPARE(runs[2].ink, RunLabel::Ink::Dim);
}

void TestMainWindow::dictionaryArticleTagTipsPairPerTag() {
    // Article pos/field split like the table: concatenated text
    // unchanged, each run carrying only its own description.
    SenseDisplay sense;
    sense.gloss = QStringLiteral("to eat");
    sense.pos = QStringList{QStringLiteral("v1"), QStringLiteral("vt")};
    sense.posTips = QStringList{QStringLiteral("Ichidan verb"), QStringLiteral("transitive")};
    sense.field = QStringList{QStringLiteral("food")};
    sense.fieldTips = QStringList{QStringLiteral("food term")};
    const QVector<RunLabel::Run> runs = ArticleRuns::senseRuns(sense, 1);
    // Number, v1, vt, gloss, field.
    QCOMPARE(runs.size(), 5);
    QCOMPARE(runs[1].text, QStringLiteral("v1, "));
    QCOMPARE(runs[1].ink, RunLabel::Ink::Green);
    QCOMPARE(runs[1].tip, QStringLiteral("Ichidan verb"));
    QCOMPARE(runs[2].text, QStringLiteral("vt "));
    QCOMPARE(runs[2].ink, RunLabel::Ink::Green);
    QCOMPARE(runs[2].tip, QStringLiteral("transitive"));
    QCOMPARE(runs[3].text, QStringLiteral("to eat"));
    QCOMPARE(runs[4].text, QStringLiteral(" food"));
    QCOMPARE(runs[4].ink, RunLabel::Ink::Link);
    QCOMPARE(runs[4].tip, QStringLiteral("food term"));
}

void TestMainWindow::dictionaryArticlePaintsRunInks() {
    // Pixel-level: run inks must reach the screen, not just the
    // format ranges. QTextLayout::draw() with an explicit formats
    // vector drops every range on this Qt version (and setFormats
    // clears laid-out lines), so formats alone prove nothing —
    // only pixels do.
    RunLabel label;
    label.setRuns(
        {{QStringLiteral("7. "), RunLabel::Ink::Body, false, true, {}},
         {QStringLiteral("adj-no "), RunLabel::Ink::Green, true, false, {}},
         {QStringLiteral("kana "), RunLabel::Ink::Red, true, false, {}},
         {QStringLiteral("col "), RunLabel::Ink::Link, true, false, {}},
         {QStringLiteral("money; coin"), RunLabel::Ink::Body},
         {QStringLiteral("\nSee also: "), RunLabel::Ink::Dim, true, false, {}},
         {QStringLiteral("お足"), RunLabel::Ink::Link, true, false, QStringLiteral("お足")}});
    label.resize(400, 200);
    label.show();
    QVERIFY(QTest::qWaitForWindowExposed(&label));
    QTest::qWait(300);
    const QImage pixels = label.grab().toImage();
    int green = 0, blue = 0, white = 0, gray = 0, red = 0;
    for (int y = 0; y < pixels.height(); ++y) {
        for (int x = 0; x < pixels.width(); ++x) {
            const QColor c = pixels.pixelColor(x, y);
            // Green dominance (not an absolute level): the light-theme
            // pos ink #2E7D32 peaks at green=125, so green>150 only
            // catches antialiased blends — zero core pixels on
            // font engines that don't smooth (Windows offscreen).
            if (c.green() >= 100 && c.green() - c.red() >= 40 && c.green() > c.blue()) {
                ++green;
            } else if (c.blue() > 180 && c.red() < 150 && c.blue() - c.red() > 40) {
                ++blue;
            } else if (c.red() > 150 && c.green() < 100 && c.blue() < 100) {
                ++red;
            } else if (c.red() > 200 && c.green() > 200 && c.blue() > 200) {
                ++white;
            } else if (qAbs(c.red() - c.green()) < 12 && qAbs(c.green() - c.blue()) < 12 &&
                       c.red() > 40 && c.red() < 200) {
                ++gray;
            }
        }
    }
    qDebug() << "PIXELS green=" << green << "blue=" << blue << "white=" << white << "gray=" << gray
             << "red=" << red;
    // Small-size tag glyphs leave few fully-saturated pixels;
    // thresholds stay an order of magnitude below measured counts.
    QVERIFY2(green > 5, "pos ink missing from paint");
    QVERIFY2(blue > 50, "link ink missing from paint");
    QVERIFY2(white > 20, "body ink missing from paint");
    QVERIFY2(red > 5, "warning ink missing from paint");
}

void TestMainWindow::dictionaryArticleTagTooltipShowsDescription() {
    // Builder level: article tag runs carry their descriptions.
    SenseDisplay sense;
    sense.gloss = QStringLiteral("to eat");
    sense.pos = QStringList{QStringLiteral("v1")};
    sense.posTips = QStringList{QStringLiteral("Ichidan verb")};
    sense.misc = QStringList{QStringLiteral("uk")};
    sense.miscTips = QStringList{QStringLiteral("usually kana")};
    const QVector<RunLabel::Run> runs = ArticleRuns::senseRuns(sense, 1);
    QString posTip;
    QString ukTip;
    for (const RunLabel::Run &run : runs) {
        if (run.text.startsWith(QStringLiteral("v1"))) {
            posTip = run.tip;
        }
        if (run.text.startsWith(QStringLiteral("kana"))) {
            ukTip = run.tip;
        }
    }
    QCOMPARE(posTip, QStringLiteral("Ichidan verb"));
    QCOMPARE(ukTip, QStringLiteral("usually kana"));
    // Widget level: hovering the tag shows the native tip after
    // the delay, like the table's tag tooltips.
    RunLabel label;
    label.setRuns({{QStringLiteral("v1 "),
                    RunLabel::Ink::Green,
                    true,
                    false,
                    {},
                    QStringLiteral("Ichidan verb")}});
    label.resize(400, qMax(1, label.heightForWidth(400)));
    label.show();
    QVERIFY(QTest::qWaitForWindowExposed(&label));
    QTRY_VERIFY_WITH_TIMEOUT(label.height() > 0, 5000);
    QMouseEvent hover(QEvent::MouseMove, QPointF(4, label.height() / 2), Qt::NoButton, Qt::NoButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&label, &hover);
    QTRY_VERIFY_WITH_TIMEOUT(QToolTip::isVisible(), 5000);
    QCOMPARE(QToolTip::text(), QStringLiteral("Ichidan verb"));
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(&label, &leave);
    QTRY_VERIFY_WITH_TIMEOUT(!QToolTip::isVisible(), 5000);
}

void TestMainWindow::dictionaryArticleCopiesSelection() {
    // Press-drag across a sense line selects text; Ctrl+C copies
    // the selection (parity with the old browser pane).
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *article = window->findChild<ArticleView *>("articleView");
    QVERIFY2(article != nullptr, "article pane missing");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    QSignalSpy finished(qobject_cast<DictionaryModel *>(table->model()),
                        &DictionaryModel::searchFinished);
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 30000);
    table->selectRow(0);
    QTRY_VERIFY_WITH_TIMEOUT(article->isVisible(), 5000);
    const QList<RunLabel *> senses =
        article->findChildren<RunLabel *>(QStringLiteral("articleSense"));
    QVERIFY(!senses.isEmpty());
    RunLabel *sense = senses.first();
    QTRY_VERIFY_WITH_TIMEOUT(sense->height() > 0, 5000);
    const int y = sense->height() / 2;
    QTest::mousePress(sense, Qt::LeftButton, Qt::NoModifier, QPoint(4, y));
    QTest::mouseMove(sense, QPoint(sense->width() - 5, y));
    QTest::mouseRelease(sense, Qt::LeftButton, Qt::NoModifier, QPoint(sense->width() - 5, y));
    const QString selected = sense->selectedText();
    QVERIFY(!selected.isEmpty());
    QTest::keyClick(sense, Qt::Key_C, Qt::ControlModifier);
    QTRY_VERIFY_WITH_TIMEOUT(QApplication::clipboard()->text() == selected, 5000);
}

void TestMainWindow::dictionaryRubySelectsKeb() {
    // Heading selection copies the written form (keb), never
    // display romaji: builder carries raw segments, the label maps
    // display offsets back (verbatim spans slice, converted spans
    // snap whole), double-click takes all of it.
    const ReadingDisplay entry = AppSettings::shared().reading();
    AppSettings::shared().setReading(ReadingDisplay::Hepburn);
    ArticleDisplay article;
    article.heading = QStringLiteral("食べる");
    article.reading = QStringLiteral("たべる");
    const QVector<RubyLabel::Span> spans = ArticleRuns::headingSpans(article);
    QString keb;
    for (const RubyLabel::Span &span : spans) {
        keb += span.keb.isEmpty() ? span.base : span.keb;
    }
    QCOMPARE(keb, QStringLiteral("食べる"));
    RubyLabel label;
    label.setSpans({{QStringLiteral("食"), QStringLiteral("た"), QStringLiteral("食")},
                    {QStringLiteral("beru"), QStringLiteral(""), QStringLiteral("べる")}});
    label.resize(400, 100);
    label.show();
    QVERIFY(QTest::qWaitForWindowExposed(&label));
    QTRY_VERIFY_WITH_TIMEOUT(label.height() > 0, 5000);
    QCOMPARE(label.text(), QStringLiteral("食beru"));
    // Double-click: the whole keb (explicit dbl-click event:
    // QTest's press/release sequence collapses the selection).
    const QPoint middle(label.width() / 2, label.height() / 2);
    QMouseEvent dblClick(QEvent::MouseButtonDblClick, QPointF(middle), Qt::LeftButton,
                         Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&label, &dblClick);
    QCOMPARE(label.selectedText(), QStringLiteral("食べる"));
    // Drag across the romaji span only: the whole kana segment.
    const int last = static_cast<int>(label.text().size()) - 1;
    QTest::mousePress(&label, Qt::LeftButton, Qt::NoModifier, label.characterRect(2).center());
    QTest::mouseMove(&label, label.characterRect(last).center());
    QTest::mouseRelease(&label, Qt::LeftButton, Qt::NoModifier, label.characterRect(last).center());
    QCOMPARE(label.selectedText(), QStringLiteral("べる"));
    // Drag across everything: full keb, not display text.
    QTest::mousePress(&label, Qt::LeftButton, Qt::NoModifier, label.characterRect(0).center());
    QTest::mouseMove(&label, label.characterRect(last).center());
    QTest::mouseRelease(&label, Qt::LeftButton, Qt::NoModifier, label.characterRect(last).center());
    QCOMPARE(label.selectedText(), QStringLiteral("食べる"));
    // End to end under Hepburn: real spans, double-click, keb out.
    RubyLabel headed;
    headed.setSpans(spans);
    headed.resize(400, 100);
    headed.show();
    QVERIFY(QTest::qWaitForWindowExposed(&headed));
    QTRY_VERIFY_WITH_TIMEOUT(headed.height() > 0, 5000);
    const QPoint headedMiddle(headed.width() / 2, headed.height() / 2);
    QMouseEvent headedDbl(QEvent::MouseButtonDblClick, QPointF(headedMiddle), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&headed, &headedDbl);
    QCOMPARE(headed.selectedText(), QStringLiteral("食べる"));
    AppSettings::shared().setReading(entry);
}

void TestMainWindow::dictionaryHeadingSelectsFully() {
    // Headword drag-select reaches past the kanji-okurigana
    // boundary to the end (cursor semantics: the end offset is a
    // valid selection edge, glyph consumers guard below it).
    ArticleDisplay article;
    article.heading = QStringLiteral("食べる");
    article.reading = QStringLiteral("たべる");
    RubyLabel label;
    label.setSpans(ArticleRuns::headingSpans(article));
    label.resize(400, 100);
    label.show();
    QVERIFY(QTest::qWaitForWindowExposed(&label));
    QTRY_VERIFY_WITH_TIMEOUT(label.height() > 0, 5000);
    QCOMPARE(label.text(), QStringLiteral("食べる"));
    const int last = static_cast<int>(label.text().size()) - 1;
    // Exact drag first char to last char, then overshooting past
    // the end like a real drag: both select the whole headword.
    QTest::mousePress(&label, Qt::LeftButton, Qt::NoModifier, label.characterRect(0).center());
    QTest::mouseMove(&label, label.characterRect(last).center());
    QTest::mouseRelease(&label, Qt::LeftButton, Qt::NoModifier, label.characterRect(last).center());
    QCOMPARE(label.selectedText(), QStringLiteral("食べる"));
    // Overshooting past the edge keeps the last valid edge (sticky)
    // instead of clearing: press, sweep through the last glyph,
    // then past the label.
    QTest::mousePress(&label, Qt::LeftButton, Qt::NoModifier, label.characterRect(0).center());
    QTest::mouseMove(&label, label.characterRect(last).center());
    QTest::mouseMove(&label, QPoint(label.width() - 1, label.height() / 2));
    QTest::mouseRelease(&label, Qt::LeftButton, Qt::NoModifier,
                        QPoint(label.width() - 1, label.height() / 2));
    QCOMPARE(label.selectedText(), QStringLiteral("食べる"));
}
