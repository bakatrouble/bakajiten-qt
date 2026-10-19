#include "TestMainWindow.h"

#include <QtTest>

#include <QApplication>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalSpy>
#include <QToolTip>

#include "core/dictionary/DictionaryModel.h"
#include "core/dictionary/MeaningBuilder.h"
#include "ui/mainwindow/MainWindow.h"
#include "util/CellText.h"
#include "widgets/table/DictionaryTable.h"

#include <memory>

void TestMainWindow::dictionaryTagTooltipShowsDescription() {
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
    QSignalSpy finished(dictModel, &DictionaryModel::searchFinished);
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 30000);
    int taberu = -1;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (model->index(row, 0).data(Qt::DisplayRole).toString() == QStringLiteral("食べる")) {
            taberu = row;
            break;
        }
    }
    QVERIFY2(taberu >= 0, "no 食べる row for たべる");
    // First tag run with a description, positioned by the same run
    // geometry paint and hit-testing share.
    const QVector<SenseDisplay> senses =
        model->index(taberu, 2).data(DictionaryModel::SensesRole).value<QVector<SenseDisplay>>();
    const QString inflection =
        model->index(taberu, 2).data(DictionaryModel::InflectionRole).toString();
    const QVector<MeaningBuilder::Run> runs =
        MeaningBuilder::build(senses, table->font(), inflection, table->palette(), false);
    const QRect cell = table->visualRect(model->index(taberu, 2));
    QVERIFY(!cell.isNull());
    int x = CellText::textRect(cell).left();
    QPoint tagAt(-1, -1);
    for (const MeaningBuilder::Run &run : runs) {
        const int w = QFontMetrics(run.font).horizontalAdvance(run.text);
        if (!run.tip.isEmpty()) {
            tagAt = QPoint(x + w / 2, cell.center().y());
            break;
        }
        x += w;
    }
    QVERIFY2(tagAt.x() >= 0, "no tag run with a description");
    QMouseEvent hover(QEvent::MouseMove, QPointF(tagAt), Qt::NoButton, Qt::NoButton,
                      Qt::NoModifier);
    QApplication::sendEvent(table->viewport(), &hover);
    QTRY_VERIFY_WITH_TIMEOUT(QToolTip::isVisible(), 5000);
    QVERIFY(QToolTip::text().contains(QStringLiteral("Ichidan verb")));
    // Leaving the viewport dismisses it.
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(table->viewport(), &leave);
    QTRY_VERIFY_WITH_TIMEOUT(!QToolTip::isVisible(), 5000);
}

void TestMainWindow::dictionaryTagTooltipSurvivesWithinRunMoves() {
    // Moving inside the same tag run must not dismiss the visible
    // tip (no leave, no run change: nothing to re-arm).
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
    QSignalSpy finished(dictModel, &DictionaryModel::searchFinished);
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 30000);
    int taberu = -1;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (model->index(row, 0).data(Qt::DisplayRole).toString() == QStringLiteral("食べる")) {
            taberu = row;
            break;
        }
    }
    QVERIFY2(taberu >= 0, "no 食べる row for たべる");
    // First tag run with a description, with its pixel span.
    const QVector<SenseDisplay> senses =
        model->index(taberu, 2).data(DictionaryModel::SensesRole).value<QVector<SenseDisplay>>();
    const QString inflection =
        model->index(taberu, 2).data(DictionaryModel::InflectionRole).toString();
    const QVector<MeaningBuilder::Run> runs =
        MeaningBuilder::build(senses, table->font(), inflection, table->palette(), false);
    const QRect cell = table->visualRect(model->index(taberu, 2));
    QVERIFY(!cell.isNull());
    int x = CellText::textRect(cell).left();
    int runFrom = -1;
    int runTo = -1;
    for (const MeaningBuilder::Run &run : runs) {
        const int w = QFontMetrics(run.font).horizontalAdvance(run.text);
        if (!run.tip.isEmpty()) {
            runFrom = x;
            runTo = x + w;
            break;
        }
        x += w;
    }
    QVERIFY2(runFrom >= 0 && runTo - runFrom > 8, "no wide tag run with a description");
    const auto moveTo = [&](int px) {
        QMouseEvent hover(QEvent::MouseMove, QPointF(px, cell.center().y()), Qt::NoButton,
                          Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(table->viewport(), &hover);
    };
    moveTo((runFrom + runTo) / 2);
    QTRY_VERIFY_WITH_TIMEOUT(QToolTip::isVisible(), 5000);
    // Two small steps inside the same run: the tip stands (it only
    // re-arms on run changes).
    moveTo(runFrom + 2);
    QTest::qWait(200);
    QVERIFY(QToolTip::isVisible());
    moveTo(runTo - 3);
    QTest::qWait(200);
    QVERIFY(QToolTip::isVisible());
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(table->viewport(), &leave);
}

void TestMainWindow::dictionaryTagTooltipSurvivesRowSweep() {
    // Same abbreviation text on adjacent rows (a column of "vt"):
    // sweeping down must not blink the tip out — the description is
    // identical, so there is nothing to re-arm. Polls for any dip:
    // pre-fix the row change hides synchronously and the 400ms
    // delay re-shows, so sampling catches the blackout. Substring
    // たべ (both toggles) yields dozens of rows, so a shared-tip
    // column exists to sweep.
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *suffix = window->findChild<QPushButton *>("suffixButton");
    QVERIFY2(suffix != nullptr, "suffix button missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    auto *dictModel = qobject_cast<DictionaryModel *>(model);
    QVERIFY2(dictModel != nullptr, "not a DictionaryModel");
    QSignalSpy finished(dictModel, &DictionaryModel::searchFinished);
    QTest::mouseClick(suffix, Qt::LeftButton);
    input->setText(QStringLiteral("たべ"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 30000);
    // Tipped x-spans per row, by the same run geometry paint uses.
    struct Span {
        int row = -1;
        int from = 0;
        int to = 0;
        QString tip;
    };
    QVector<Span> spans;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex at = model->index(row, 2);
        if (table->visualRect(at).isNull()) {
            continue;
        }
        const QVector<SenseDisplay> senses =
            at.data(DictionaryModel::SensesRole).value<QVector<SenseDisplay>>();
        const QString inflection = at.data(DictionaryModel::InflectionRole).toString();
        const QVector<MeaningBuilder::Run> runs =
            MeaningBuilder::build(senses, table->font(), inflection, table->palette(), false);
        const QRect cell = table->visualRect(at);
        int x = CellText::textRect(cell).left();
        for (const MeaningBuilder::Run &run : runs) {
            const int w = QFontMetrics(run.font).horizontalAdvance(run.text);
            if (!run.tip.isEmpty()) {
                spans << Span{row, x, x + w, run.tip};
            }
            x += w;
        }
    }
    // Adjacent visible rows sharing one x inside equally-tipped runs.
    int sweepX = -1;
    int firstRow = -1;
    for (int i = 1; i < spans.size(); ++i) {
        if (spans[i].row == spans[i - 1].row + 1 && spans[i].tip == spans[i - 1].tip) {
            const int from = qMax(spans[i].from, spans[i - 1].from);
            const int to = qMin(spans[i].to, spans[i - 1].to);
            if (to - from > 8) {
                sweepX = (from + to) / 2;
                firstRow = spans[i - 1].row;
                break;
            }
        }
    }
    QVERIFY2(sweepX >= 0, "no shared-tip tag column on adjacent rows");
    const auto moveTo = [&](int row) {
        const QRect cell = table->visualRect(model->index(row, 2));
        QMouseEvent hover(QEvent::MouseMove, QPointF(sweepX, cell.center().y()), Qt::NoButton,
                          Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(table->viewport(), &hover);
    };
    moveTo(firstRow);
    QTRY_VERIFY_WITH_TIMEOUT(QToolTip::isVisible(), 5000);
    // Down one row, same abbreviation: sample for a full second —
    // any hide/show blink fails the run.
    moveTo(firstRow + 1);
    for (int i = 0; i < 20; ++i) {
        QTest::qWait(50);
        QVERIFY2(QToolTip::isVisible(), "tip blinked out mid-sweep");
    }
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(table->viewport(), &leave);
}
