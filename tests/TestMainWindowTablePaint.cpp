#include "TestMainWindow.h"

#include <QtTest>

#include <QFont>
#include <QLineEdit>
#include <QPalette>
#include <QSignalSpy>

#include "core/dictionary/DictionaryModel.h"
#include "core/dictionary/MeaningBuilder.h"
#include "core/palette/MaterialPalette.h"
#include "ui/mainwindow/MainWindow.h"
#include "util/CellText.h"
#include "widgets/table/DictionaryTable.h"

#include <memory>

void TestMainWindow::dictionaryMeaningLayoutElidesOverflow() {
    // Shared run layout: positions map inside the drawn (elided)
    // text, so hover/drag math can never point at invisible runs.
    // Pure unit test over the builder (no backend, no pixels).
    MeaningBuilder::Run pos;
    pos.text = QStringLiteral("v1, vt");
    pos.font = QFont();
    pos.kind = MeaningBuilder::Kind::Pos;
    pos.tip = QStringLiteral("Ichidan verb");
    MeaningBuilder::Run gloss;
    gloss.text = QStringLiteral("to eat a lot of food every day");
    gloss.font = QFont();
    gloss.kind = MeaningBuilder::Kind::Gloss;
    const QVector<MeaningBuilder::Run> runs{pos, gloss};
    // Wide cell: nothing elides, full widths.
    const int posW = QFontMetrics(pos.font).horizontalAdvance(pos.text);
    const int glossW = QFontMetrics(gloss.font).horizontalAdvance(gloss.text);
    const QVector<MeaningBuilder::PlacedRun> wide =
        MeaningBuilder::layoutRuns(runs, {10, 10 + posW + glossW + 20});
    QCOMPARE(wide.size(), 2);
    QCOMPARE(wide[0].x, 10);
    QCOMPARE(wide[0].shown, pos.text);
    QCOMPARE(wide[1].x, 10 + posW);
    QCOMPARE(wide[1].shown, gloss.text);
    // Squeezed cell: the gloss ellipsizes, positions stay inside
    // the drawn text (the old full-width walk pointed past it).
    // The squeeze derives from live metrics — room for one glyph
    // plus the ellipsis, minus a pixel — so elision is forced on
    // any font instead of assuming a fixed pixel budget.
    const QFontMetrics glossMetrics(gloss.font);
    const int oneW = glossMetrics.horizontalAdvance(gloss.text.left(1));
    const int ellW = glossMetrics.horizontalAdvance(QChar(0x2026));
    const int narrowRight = 10 + posW + oneW + ellW - 1;
    const QVector<MeaningBuilder::PlacedRun> narrow =
        MeaningBuilder::layoutRuns(runs, {10, narrowRight});
    QCOMPARE(narrow.size(), 2);
    QCOMPARE(narrow[0].x, 10);
    QCOMPARE(narrow[0].shown, pos.text);
    QVERIFY(narrow[1].shown.endsWith(QChar(0x2026)));
    QVERIFY(narrow[1].shown.size() < gloss.text.size());
    const int tailX = narrow[1].x + QFontMetrics(gloss.font).horizontalAdvance(narrow[1].shown) - 1;
    QVERIFY(tailX <= narrowRight);
    QVERIFY(tailX >= narrow[1].x);
}

void TestMainWindow::dictionaryMeaningTagsShareBaseline() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    // Direct hit (no deinflected tint): plain base behind the ink.
    QSignalSpy finished(qobject_cast<DictionaryModel *>(model), &DictionaryModel::searchFinished);
    input->setText(QStringLiteral("食べる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 30000);
    int taberu = -1;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (model->index(row, 0).data(Qt::DisplayRole).toString() == QStringLiteral("食べる")) {
            taberu = row;
            break;
        }
    }
    QVERIFY2(taberu >= 0, "no 食べる row for 食べる");
    // First pos tag run and first gloss run, positioned by the
    // same run geometry paint uses. Both are descender-free
    // ("v1, vt" / "to eat"), so a shared baseline means equal
    // bottom ink — centered tags would sit higher.
    const QVector<SenseDisplay> senses =
        model->index(taberu, 2).data(DictionaryModel::SensesRole).value<QVector<SenseDisplay>>();
    const QString inflection =
        model->index(taberu, 2).data(DictionaryModel::InflectionRole).toString();
    const QVector<MeaningBuilder::Run> runs =
        MeaningBuilder::build(senses, table->font(), inflection, table->palette(), false);
    table->scrollTo(model->index(taberu, 2), QAbstractItemView::EnsureVisible);
    const QRect cell = table->visualRect(model->index(taberu, 2));
    QVERIFY(!cell.isNull());
    const QImage painted = table->viewport()->grab().toImage();
    const QColor base = table->palette().color(QPalette::Base);
    int x = CellText::textRect(cell).left();
    int tagFrom = -1;
    int tagTo = -1;
    int glossFrom = -1;
    int glossTo = -1;
    // Below-baseline glyphs (descenders, commas) hang past any
    // baseline, so the tag range stops before the first of them —
    // the comparison only ever sees baseline-sitting ink.
    const auto cleanEnd = [](const QString &text) {
        int end = text.size();
        for (const QChar risky : {u',', u';', u':', u'g', u'j', u'p', u'q', u'y'}) {
            const int at = text.indexOf(risky);
            if (at >= 0) {
                end = qMin(end, at);
            }
        }
        return end;
    };
    for (const MeaningBuilder::Run &run : runs) {
        const int end = cleanEnd(run.text);
        const int w = QFontMetrics(run.font).horizontalAdvance(run.text.left(end));
        if (tagFrom < 0 && run.kind == MeaningBuilder::Kind::Pos && end > 0) {
            tagFrom = x;
            tagTo = x + w;
        } else if (tagTo >= 0 && glossFrom < 0 && run.kind == MeaningBuilder::Kind::Gloss) {
            const int glossEnd = cleanEnd(run.text);
            if (glossEnd > 0) {
                glossFrom = x;
                glossTo = x + QFontMetrics(run.font).horizontalAdvance(run.text.left(glossEnd));
                break;
            }
        }
        x += QFontMetrics(run.font).horizontalAdvance(run.text);
    }
    QVERIFY2(tagFrom >= 0 && glossFrom >= 0, "no tag+gloss run pair");
    const auto colorDistance = [](const QColor &a, const QColor &b) {
        return qAbs(a.red() - b.red()) + qAbs(a.green() - b.green()) + qAbs(a.blue() - b.blue());
    };
    const auto bottomInk = [&](int from, int to) {
        for (int y = cell.bottom(); y >= cell.top(); --y) {
            for (int x = from; x < to && x <= cell.right(); ++x) {
                // Core ink only: antialias fringes differ per font
                // size and would fail an exact-bottom compare.
                if (colorDistance(painted.pixelColor(x, y), base) >= 64) {
                    return y;
                }
            }
        }
        return -1;
    };
    const int tagBottom = bottomInk(tagFrom, tagTo);
    const int glossBottom = bottomInk(glossFrom, glossTo);
    QVERIFY2(tagBottom > cell.center().y(), "no tag ink found");
    QVERIFY2(glossBottom > cell.center().y(), "no gloss ink found");
    QCOMPARE(tagBottom, glossBottom);
}

void TestMainWindow::dictionaryKanaOnlyHeadingTransparent() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    auto *dictModel = qobject_cast<DictionaryModel *>(model);
    QVERIFY2(dictModel != nullptr, "not a DictionaryModel");
    // ちょっと: 一寸 is usually-kana (first-sense `uk`), ちょっと見
    // is not.
    QSignalSpy finished(dictModel, &DictionaryModel::searchFinished);
    input->setText(QStringLiteral("ちょっと"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 30000);
    const auto kanaOnlyAt = [&](int row) {
        return model->index(row, 0).data(DictionaryModel::KanaOnlyRole).toBool();
    };
    int kana = -1;
    int kanji = -1;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QString heading = model->index(row, 0).data(Qt::DisplayRole).toString();
        if (heading == QStringLiteral("一寸") && kana < 0) {
            kana = row;
        }
        if (!kanaOnlyAt(row) && kanji < 0) {
            kanji = row;
        }
    }
    QVERIFY2(kana >= 0, "no 一寸 row for ちょっと");
    QVERIFY(kanaOnlyAt(kana));
    QVERIFY2(kanji >= 0, "no non-kana row for ちょっと");
    // Faded heading ink on the kana row only (backgrounds stay
    // the base everywhere: the marker is text transparency, not a
    // tint). One grab per row — a later scroll can move an earlier
    // row out of the viewport. The core glyph pixel (farthest from
    // the base) is the solid pen on opaque rows, pen-over-base
    // blend on faded ones.
    const QColor base = table->palette().color(QPalette::Base);
    const QColor text = table->palette().color(QPalette::Text);
    const auto coreInk = [&](int row) {
        table->scrollTo(model->index(row, 0), QAbstractItemView::EnsureVisible);
        const QRect cell = table->visualRect(model->index(row, 0));
        if (cell.isNull() || !table->viewport()->rect().intersects(cell)) {
            QTest::qFail("heading cell not visible", __FILE__, __LINE__);
            return base;
        }
        const QImage painted = table->viewport()->grab().toImage();
        const QRect rect = CellText::textRect(cell).intersected(painted.rect());
        QColor best = base;
        int bestDist = -1;
        for (int y = rect.top(); y <= rect.bottom(); ++y) {
            for (int x = rect.left(); x <= rect.right(); ++x) {
                const QColor pixel = painted.pixelColor(x, y);
                const int dist = qAbs(pixel.red() - base.red()) +
                                 qAbs(pixel.green() - base.green()) +
                                 qAbs(pixel.blue() - base.blue());
                if (dist > bestDist) {
                    bestDist = dist;
                    best = pixel;
                }
            }
        }
        if (bestDist < 0) {
            QTest::qFail("no heading ink found", __FILE__, __LINE__);
            return base;
        }
        return best;
    };
    const QColor solid = coreInk(kanji);
    QVERIFY(solid != base);
    QCOMPARE(solid, text);
    const QColor faded = coreInk(kana);
    QVERIFY(faded != base);
    QVERIFY(faded != text);
}

void TestMainWindow::dictionaryUkTagRendersRed() {
    // Misc tags split per tag: only `uk` reads warning-red, the
    // rest stay dim, each keeping its own tooltip.
    SenseDisplay sense;
    sense.gloss = QStringLiteral("to eat");
    sense.misc = QStringList{QStringLiteral("uk"), QStringLiteral("arch")};
    sense.miscTips = QStringList{QStringLiteral("usually kana"), QStringLiteral("archaic")};
    const QPalette palette;
    const MaterialPalette theme = MaterialPalette::fromPalette(palette);
    const QVector<MeaningBuilder::Run> runs =
        MeaningBuilder::build({sense}, QFont(), QString(), palette, false);
    // Number, uk, arch, gloss, gap.
    QCOMPARE(runs.size(), 5);
    QCOMPARE(runs[1].text, QStringLiteral("kana, "));
    QCOMPARE(runs[1].kind, MeaningBuilder::Kind::Uk);
    QCOMPARE(runs[1].pen, theme.warning());
    QCOMPARE(runs[1].tip, QStringLiteral("usually kana"));
    QCOMPARE(runs[2].text, QStringLiteral("arch "));
    QCOMPARE(runs[2].kind, MeaningBuilder::Kind::Misc);
    QCOMPARE(runs[2].pen, theme.dimmed());
    QCOMPARE(runs[2].tip, QStringLiteral("archaic"));
}

void TestMainWindow::dictionaryTagTipsPairPerTag() {
    // pos/field split like misc: concatenated text unchanged, each
    // run carrying only its own description.
    SenseDisplay sense;
    sense.gloss = QStringLiteral("to eat");
    sense.pos = QStringList{QStringLiteral("v1"), QStringLiteral("vt")};
    sense.posTips = QStringList{QStringLiteral("Ichidan verb"), QStringLiteral("transitive")};
    sense.field = QStringList{QStringLiteral("food")};
    sense.fieldTips = QStringList{QStringLiteral("food term")};
    const QPalette palette;
    const QVector<MeaningBuilder::Run> runs =
        MeaningBuilder::build({sense}, QFont(), QString(), palette, false);
    // Number, v1, vt, gloss, field, gap.
    QCOMPARE(runs.size(), 6);
    QCOMPARE(runs[1].text, QStringLiteral("v1, "));
    QCOMPARE(runs[1].kind, MeaningBuilder::Kind::Pos);
    QCOMPARE(runs[1].tip, QStringLiteral("Ichidan verb"));
    QCOMPARE(runs[2].text, QStringLiteral("vt "));
    QCOMPARE(runs[2].kind, MeaningBuilder::Kind::Pos);
    QCOMPARE(runs[2].tip, QStringLiteral("transitive"));
    QCOMPARE(runs[3].text, QStringLiteral("to eat"));
    QCOMPARE(runs[4].text, QStringLiteral(" food"));
    QCOMPARE(runs[4].kind, MeaningBuilder::Kind::Field);
    QCOMPARE(runs[4].tip, QStringLiteral("food term"));
}
