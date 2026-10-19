#include "TestMainWindow.h"

#include <QtTest>

#include <QApplication>
#include <QCursor>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>

#include "core/dictionary/DictionaryModel.h"
#include "core/dictionary/KanjiInfo.h"
#include "core/palette/MaterialPalette.h"
#include "ui/mainwindow/MainWindow.h"
#include "widgets/table/DictionaryTable.h"
#include "widgets/text/ArticleView.h"
#include "widgets/text/RubyLabel.h"
#include "widgets/text/RunLabel.h"
#include "widgets/tooltip/KanjiTooltip.h"
#include "widgets/tooltip/Tooltip.h"

#include <QScrollBar>
#include <memory>

void TestMainWindow::dictionaryKanjiHoverShowsInfo() {
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
    // Hover the 食 glyph (first char of the heading cell): short
    // info card with readings and stroke count. Hand-built move
    // events (QTest::mouseMove leaves phantom moves behind). One
    // shared window-level card (see KanjiHover).
    auto *tip = window->findChild<QWidget *>("kanjiTip");
    QVERIFY2(tip != nullptr, "kanji hover card missing");
    const QRect cell = table->visualRect(model->index(0, 0));
    QVERIFY(!cell.isNull());
    const QPoint at = cell.topLeft() + QPoint(6, cell.height() / 2);
    QMouseEvent hover(QEvent::MouseMove, QPointF(at), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(table->viewport(), &hover);
    QTest::qWait(450);
    QVERIFY(tip->isVisible());
    QVERIFY(tip->windowFlags() & Qt::ToolTip);
    auto *glyph = tip->findChild<QLabel *>("kanjiGlyph");
    QVERIFY2(glyph != nullptr, "kanji glyph missing");
    QCOMPARE(glyph->text(), QStringLiteral("食"));
    auto *strokes = tip->findChild<QLabel *>("kanjiStrokes");
    QVERIFY2(strokes != nullptr, "strokes value missing");
    QCOMPARE(strokes->text(), QStringLiteral("9"));
    auto *radical = tip->findChild<QLabel *>("kanjiRadical");
    QVERIFY2(radical != nullptr, "radical glyph missing");
    QCOMPARE(radical->text(), KanjiInfo::radicalGlyph(184));
    auto *meanings = tip->findChild<QLabel *>("kanjiMeanings");
    QVERIFY2(meanings != nullptr, "meanings missing");
    QVERIFY(meanings->text().contains(QStringLiteral("eat")));
    auto *on = tip->findChild<QLabel *>("kanjiOn");
    QVERIFY2(on != nullptr, "on readings missing");
    QVERIFY(on->text().contains(QStringLiteral("ショク")));
    auto *grade = tip->findChild<QLabel *>("kanjiGrade");
    QVERIFY2(grade != nullptr, "grade missing");
    QCOMPARE(grade->text(), QStringLiteral("Kyouiku: 2"));
    // Kana hover dismisses (reading cell has no kanji).
    const QRect reading = table->visualRect(model->index(0, 1));
    QMouseEvent hoverKana(QEvent::MouseMove, QPointF(reading.center()), Qt::NoButton, Qt::NoButton,
                          Qt::NoModifier);
    QApplication::sendEvent(table->viewport(), &hoverKana);
    QTRY_VERIFY_WITH_TIMEOUT(!tip->isVisible(), 5000);
    // Same card from the article pane (heading glyph).
    QTest::mouseClick(table->viewport(), Qt::LeftButton, {}, cell.center());
    QTRY_VERIFY_WITH_TIMEOUT(article->isVisible(), 5000);
    auto *articleTip = window->findChild<QWidget *>("kanjiTip");
    QVERIFY2(articleTip != nullptr, "article hover card missing");
    auto *heading = article->findChild<RubyLabel *>(QStringLiteral("articleHeading"));
    QVERIFY2(heading != nullptr, "article heading missing");
    // Hover point from the label's own hit-testing: middle of the
    // 食 base run (edges stay out — advances disagree by a few
    // pixels at glyph edges).
    const int kanjiIndex = heading->text().indexOf(QChar(u'食'));
    QVERIFY2(kanjiIndex >= 0, "no 食 in article heading");
    const QRect kanjiRect = heading->characterRect(kanjiIndex);
    QVERIFY(!kanjiRect.isNull());
    // Same card from the article pane (heading glyph). Park the
    // real cursor on the glyph first: the QTest click above parks
    // it over the table, and its delayed motion events would
    // otherwise race this synthetic hover on the one shared card.
    QCursor::setPos(heading->mapToGlobal(kanjiRect.center()));
    QMouseEvent hoverArticle(QEvent::MouseMove, QPointF(kanjiRect.center()), Qt::NoButton,
                             Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(heading, &hoverArticle);
    QTest::qWait(450);
    QTRY_VERIFY_WITH_TIMEOUT(articleTip->isVisible(), 5000);
    auto *articleGlyph = articleTip->findChild<QLabel *>("kanjiGlyph");
    QVERIFY2(articleGlyph != nullptr, "article kanji glyph missing");
    QCOMPARE(articleGlyph->text(), QStringLiteral("食"));
    auto *articleOn = articleTip->findChild<QLabel *>("kanjiOn");
    QVERIFY2(articleOn != nullptr, "article on readings missing");
    QVERIFY(articleOn->text().contains(QStringLiteral("ショク")));
}

void TestMainWindow::dictionaryArticleTipFollowsScroll() {
    // Scrolling moves content under a stationary cursor but fires no
    // MouseMove: the card must re-hit-test on scroll, matching a
    // fresh hover at the same point (stale = pre-scroll character).
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
    auto *tip = window->findChild<QWidget *>("kanjiTip");
    QVERIFY2(tip != nullptr, "kanji hover card missing");
    auto *scroll = article->verticalScrollBar();
    QVERIFY2(scroll != nullptr, "article scrollbar missing");
    auto *heading0 = article->findChild<RubyLabel *>(QStringLiteral("articleHeading"));
    QVERIFY2(heading0 != nullptr, "article heading missing");
    QTRY_VERIFY_WITH_TIMEOUT(heading0->height() > 0, 5000);
    // Shrink until scrolling provably moves the heading glyph out
    // from under a fixed point (maximum beyond the heading height;
    // roomier fonts overflow earlier with a range too small to ever
    // leave the heading, so every probe reads 食 or a gap).
    // Layout settles async; roomier fonts need a smaller window.
    window->resize(500, 350);
    QSize winSize(500, 350);
    QTest::qWait(500);
    while (winSize.width() > 280 && scroll->maximum() <= heading0->height()) {
        winSize -= QSize(80, 50);
        window->resize(winSize);
        QTest::qWait(500);
    }
    QVERIFY2(scroll->maximum() > heading0->height(),
             "article scroll range never clears the heading");
    // Hover the 食 heading glyph: the card shows 食. Point found
    // through the view's own hit-testing (mirrors a user hover).
    auto *heading = article->findChild<RubyLabel *>(QStringLiteral("articleHeading"));
    QVERIFY2(heading != nullptr, "article heading missing");
    QTRY_VERIFY_WITH_TIMEOUT(heading->height() > 0, 5000);
    const int kanjiIndex = heading->text().indexOf(QChar(u'食'));
    QVERIFY2(kanjiIndex >= 0, "no 食 in article heading");
    QPoint hoverAt =
        heading->mapTo(article->viewport(), heading->characterRect(kanjiIndex).center());
    QTRY_VERIFY(article->kanjiAt(hoverAt) == QStringLiteral("食"));
    // Synthetic moves target labels directly (viewport-targeted
    // synthetic MouseMoves never reach filters; real ones land on
    // children anyway). Park the real cursor on the point first:
    // the shrunken window can strand it outside, and its queued
    // Leave cascade would otherwise cancel the pending show on the
    // one shared card.
    QCursor::setPos(article->viewport()->mapToGlobal(hoverAt));
    // setPos may pump a settling layout: re-derive the point from
    // live geometry so the synthetic move below can't miss it.
    hoverAt = heading->mapTo(article->viewport(), heading->characterRect(kanjiIndex).center());
    QVERIFY2(article->kanjiAt(hoverAt) == QStringLiteral("食"), "hover point went stale");
    const auto hoverViewportPoint = [&](const QPoint &at) {
        // Nudge-tolerant hit: ruby/furigana runs can leave a pixel
        // gap under a different font's metrics (Hiragino vs Noto),
        // so probe the 3x3 neighborhood and take the first label.
        QWidget *label = nullptr;
        QPoint local;
        for (int dy = -1; dy <= 1 && label == nullptr; ++dy) {
            for (int dx = -1; dx <= 1 && label == nullptr; ++dx) {
                local = QPoint();
                label = article->labelAt(at + QPoint(dx, dy), &local);
            }
        }
        QVERIFY2(label != nullptr, "no label under hover point");
        QMouseEvent move(QEvent::MouseMove, QPointF(local), Qt::NoButton, Qt::NoButton,
                         Qt::NoModifier);
        QApplication::sendEvent(label, &move);
    };
    hoverViewportPoint(hoverAt);
    QTest::qWait(450);
    QTRY_VERIFY_WITH_TIMEOUT(tip->isVisible(), 5000);
    auto *glyph = tip->findChild<QLabel *>("kanjiGlyph");
    QVERIFY2(glyph != nullptr, "kanji glyph missing");
    QCOMPARE(glyph->text(), QStringLiteral("食"));
    // Scroll to a spot where the same viewport point shows something
    // else with content under it (scan down from max in fine steps
    // for the first position that moves off 食 onto another glyph —
    // guards the coincidence where the tail shows the same
    // character, and empty areas that have no label to hover; four
    // coarse fractions miss on roomier fonts where the point stays
    // on the heading or lands in gaps at every probe).
    const auto charAtPoint = [&] { return article->kanjiAt(hoverAt); };
    int target = -1;
    const int steps = 24;
    for (int i = 0; i <= steps && target < 0; ++i) {
        scroll->setValue(qRound(scroll->maximum() * (1.0 - double(i) / double(steps))));
        const QString at = charAtPoint();
        if (!at.isEmpty() && at != QStringLiteral("食")) {
            target = scroll->value();
        }
    }
    QVERIFY2(target >= 0, "scroll never moves off the heading glyph onto content");
    QVERIFY2(!article->kanjiAt(hoverAt).isEmpty(), "no content under hover point after scroll");
    // No further mouse moves: the scroll hook alone must converge to
    // exactly what a fresh hover at the same point shows.
    QTest::qWait(600);
    const bool autoVisible = tip->isVisible();
    const QString autoText = glyph->text();
    hoverViewportPoint(hoverAt);
    QTest::qWait(600);
    QCOMPARE(tip->isVisible(), autoVisible);
    QCOMPARE(glyph->text(), autoText);
    // Cursor outside (Leave) then scroll: no phantom card.
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(article->viewport(), &leave);
    QTest::qWait(500);
    QVERIFY(!tip->isVisible());
    scroll->setValue(0);
    QTest::qWait(500);
    QVERIFY(!tip->isVisible());
}

void TestMainWindow::dictionaryArticleHoverPastEndSafe() {
    // Ruby column wider than its base (long reading over one
    // kanji): hovering the right padding used to return a
    // one-past-the-end offset, tripping QString mid/at asserts.
    ArticleDisplay article;
    article.heading = QStringLiteral("食");
    article.reading = QStringLiteral("たべもの");
    ArticleView view;
    view.setArticle(article);
    view.resize(400, 300);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    auto *heading = view.findChild<RubyLabel *>(QStringLiteral("articleHeading"));
    QVERIFY2(heading != nullptr, "article heading missing");
    QTRY_VERIFY_WITH_TIMEOUT(heading->height() > 0, 5000);
    QVERIFY2(heading->width() > 0, "heading never laid out");
    // Scan to the column's trailing edge: the last x still inside
    // the ruby-wider column sits on padding past the glyph.
    // Cursor semantics allow the end offset (selection needs it);
    // glyph consumers must guard below it.
    int edgeX = -1;
    int edgeOffset = -2;
    const int midY = heading->height() / 2;
    for (int x = 0; x < heading->width(); ++x) {
        const int o = heading->characterAt(QPoint(x, midY));
        if (o >= 0) {
            edgeX = x;
            edgeOffset = o;
        }
    }
    QVERIFY2(edgeX >= 0, "column never hit");
    QVERIFY2(edgeOffset <= heading->text().size(), "hit-test ran past the end");
    // Same point through the view seam: must not assert. At the end
    // offset there is no glyph (empty, not a crash).
    const QPoint at = heading->mapTo(view.viewport(), QPoint(edgeX, 1));
    const QString kanji = view.kanjiAt(at);
    QVERIFY(kanji.isEmpty() || kanji == QStringLiteral("食"));
    // A point on the live glyph still resolves through the seam.
    const QPoint glyphAt = heading->mapTo(view.viewport(), heading->characterRect(0).center());
    QCOMPARE(view.kanjiAt(glyphAt), QStringLiteral("食"));
}

void TestMainWindow::dictionaryKanjiCardMatchesMock() {
    // Hermetic card check for 亜 (the mock character): header
    // values, readings, footer — plus the radical table itself.
    QCOMPARE(KanjiInfo::radicalGlyph(1), QStringLiteral("一"));
    QCOMPARE(KanjiInfo::radicalGlyph(7), QStringLiteral("二"));
    QCOMPARE(KanjiInfo::radicalGlyph(184), QStringLiteral("食"));
    QCOMPARE(KanjiInfo::radicalGlyph(0), QString());
    QCOMPARE(KanjiInfo::radicalGlyph(215), QString());
    KanjiInfo::Display card;
    card.valid = true;
    card.character = QStringLiteral("亜");
    card.strokes = 7;
    card.radical = 7;
    card.on = QStringList{QStringLiteral("ア")};
    card.kun = QStringList{QStringLiteral("つ.ぐ")};
    card.meanings = QStringList{QStringLiteral("Asia"), QStringLiteral("rank next"),
                                QStringLiteral("come after"), QStringLiteral("-ous")};
    card.grade = 8;
    card.jlpt = 1;
    KanjiTooltip tip;
    tip.hover(QStringLiteral("亜"), card);
    QTRY_VERIFY_WITH_TIMEOUT(tip.isVisible(), 5000);
    const auto textOf = [&](const char *name) -> QString {
        auto *label = tip.findChild<QLabel *>(QString::fromLatin1(name));
        if (label == nullptr) {
            QTest::qFail(name, __FILE__, __LINE__);
            return {};
        }
        return label->text();
    };
    QCOMPARE(textOf("kanjiGlyph"), QStringLiteral("亜"));
    QCOMPARE(textOf("kanjiStrokes"), QStringLiteral("7"));
    QCOMPARE(textOf("kanjiRadicalNumber"), QStringLiteral("7"));
    QCOMPARE(textOf("kanjiRadical"), QStringLiteral("二"));
    QCOMPARE(textOf("kanjiMeanings"), QStringLiteral("Asia, rank next, come after, -ous"));
    QCOMPARE(textOf("kanjiOn"), QStringLiteral("ア"));
    QCOMPARE(textOf("kanjiKun"), QStringLiteral("つ.ぐ"));
    QCOMPARE(textOf("kanjiGrade"), QStringLiteral("Jouyou: 8"));
    QCOMPARE(textOf("kanjiJlpt"), QStringLiteral("1"));
    // Card width follows content between the 200/400 form bounds
    // (the window is exactly the card now — no margins around it).
    const auto cardWidth = [&]() -> int {
        auto *card = tip.findChild<QWidget *>(QStringLiteral("tooltipCard"));
        if (card == nullptr) {
            QTest::qFail("tooltip card missing", __FILE__, __LINE__);
            return 0;
        }
        return card->width();
    };
    QVERIFY2(cardWidth() >= 200 && cardWidth() <= 400, "card width outside the form bounds");
    // Table order: meanings, then Kun, then ON — packed top-down
    // beside the glyph. Checked on the tall card (all rows
    // visible): hidden rows keep stale geometry, so ordering them
    // is meaningless.
    const auto rowTop = [&](const char *name) {
        auto *label = tip.findChild<QLabel *>(QString::fromLatin1(name));
        if (label == nullptr) {
            QTest::qFail(name, __FILE__, __LINE__);
            return -1;
        }
        return label->mapTo(&tip, QPoint(0, 0)).y();
    };
    const int meaningsTop = rowTop("kanjiMeanings");
    const int kunTop = rowTop("kanjiKun");
    const int onTop = rowTop("kanjiOn");
    QVERIFY(meaningsTop >= 0 && kunTop > meaningsTop && onTop > kunTop);
    // Short card shrinks: single-line meanings end up shorter
    // than the four-meaning mock above.
    const int tallHeight = tip.height();
    KanjiInfo::Display shortCard;
    shortCard.valid = true;
    shortCard.character = QStringLiteral("一");
    shortCard.strokes = 1;
    shortCard.radical = 1;
    shortCard.on = QStringList{QStringLiteral("イツ")};
    shortCard.meanings = QStringList{QStringLiteral("one")};
    tip.hover(QStringLiteral("一"), shortCard);
    QTest::qWait(500);
    // Settles asynchronously under suite load; the height gate
    // retries, the width asserts below pin the settled state.
    QTRY_VERIFY_WITH_TIMEOUT(tip.height() < tallHeight, 5000);
    // Width follows the header grid here (both cards measure the
    // same); the bounds, not an exact size, are the contract.
    QVERIFY2(cardWidth() >= 200 && cardWidth() <= 400, "card width outside the form bounds");
    // Unknown grade/JLPT read as hyphens, footer still shown.
    KanjiInfo::Display bare;
    bare.valid = true;
    bare.character = QStringLiteral("あ");
    bare.strokes = 3;
    tip.hover(QStringLiteral("あ"), bare);
    QTest::qWait(500);
    QCOMPARE(textOf("kanjiGrade"), QStringLiteral("Grade: -"));
    QCOMPARE(textOf("kanjiJlpt"), QStringLiteral("-"));
    // Separator: one 1px rule, actually painted (Sunken QFrames
    // vanish on dark cards).
    auto *rule = tip.findChild<QWidget *>(QStringLiteral("ruleBottom"));
    QVERIFY2(rule != nullptr, "rule missing");
    QCOMPARE(rule->height(), 1);
    const QImage pixels = tip.grab().toImage();
    const MaterialPalette painted = MaterialPalette::fromPalette(tip.palette());
    const QPoint at = rule->mapTo(&tip, QPoint(rule->width() / 2, 0));
    // Names, not QColor == (it distinguishes color specs).
    QCOMPARE(pixels.pixelColor(at).name(), painted.cardBorder().name());
    tip.hide();
}

void TestMainWindow::dictionaryTooltipCardAlignsToCursor() {
    // Server-drawn shadows: the window itself (exactly the card,
    // zero margins) lands at the cursor offset. Real cursor on
    // xcb: saved/restored around the check.
    const QPoint entry = QCursor::pos();
    QCursor::setPos(500, 400);
    Tooltip tip;
    QVERIFY(!(tip.windowFlags() & Qt::NoDropShadowWindowHint));
    tip.hover();
    QTRY_VERIFY_WITH_TIMEOUT(tip.isVisible(), 5000);
    auto *card = tip.findChild<QWidget *>(QStringLiteral("tooltipCard"));
    QVERIFY2(card != nullptr, "tooltip card missing");
    QVERIFY(card->graphicsEffect() == nullptr);
    // Offscreen mapToGlobal runs 2px off (probe-verified); xcb is
    // exact. Either way this stays an order of magnitude below the
    // 22px margin regression it guards.
    QVERIFY2((tip.pos() - QPoint(516, 416)).manhattanLength() <= 6, "tip off the cursor");
    const QPoint cardAt = card->mapToGlobal(QPoint(0, 0));
    QVERIFY2((cardAt - QPoint(516, 416)).manhattanLength() <= 6, "card off the cursor");
    tip.hide();
    QCursor::setPos(entry);
}

void TestMainWindow::dictionaryKanjiPartsFromData() {
    // Pure display mapping first (no artifact needed).
    QCOMPARE(KanjiInfo::jisHex(24, 62), QStringLiteral("0x385e"));
    QCOMPARE(KanjiInfo::jisHex(16, 1), QStringLiteral("0x3021"));
    // Real v6 artifact through the Qt layer (same candidate roots
    // as the app's artifact search).
    QString path;
    for (const QString &candidate :
         {QStringLiteral("data/kanji.bin"), QStringLiteral("../data/kanji.bin"),
          QStringLiteral("../../data/kanji.bin"), QStringLiteral("../../../data/kanji.bin")}) {
        if (QFile::exists(candidate)) {
            path = candidate;
            break;
        }
    }
    QVERIFY2(!path.isEmpty(), "no kanji.bin under ./ ../ ../../ ../../../data");
    KanjiInfo info;
    QVERIFY(info.load(path));
    const KanjiInfo::Display go = info.display(QStringLiteral("五"));
    QVERIFY(go.valid);
    QCOMPARE(go.parts, QStringList{QStringLiteral("五")});
    QCOMPARE(go.jis, QStringLiteral("0x385e"));
    const QStringList users = info.containing(QStringLiteral("五"));
    QVERIFY(users.contains(QStringLiteral("吾")));
    QVERIFY(users.contains(QStringLiteral("伍")));
    QVERIFY(users.contains(QStringLiteral("語")));
    // Kana is not a part; multi-char misses.
    QVERIFY(info.containing(QStringLiteral("あ")).isEmpty());
    QVERIFY(info.containing(QStringLiteral("五五")).isEmpty());
}
