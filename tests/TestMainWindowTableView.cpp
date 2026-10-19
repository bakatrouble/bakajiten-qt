#include "TestMainWindow.h"

#include <QtTest>

#include <QApplication>
#include <QLineEdit>
#include <QPalette>
#include <QTableView>

#include "core/dictionary/DictionaryModel.h"
#include "core/dictionary/MeaningBuilder.h"
#include "ui/mainwindow/MainWindow.h"
#include "widgets/table/DictionaryTable.h"
#include "widgets/table/DictionaryTableDelegate.h"

#include <QClipboard>
#include <QHeaderView>
#include <QScrollBar>
#include <memory>

void TestMainWindow::dictionaryTableScrollsSmoothlyAndStretches() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<QTableView *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing");
    QCOMPARE(table->verticalScrollMode(), QAbstractItemView::ScrollPerPixel);
    QCOMPARE(table->horizontalScrollMode(), QAbstractItemView::ScrollPerPixel);
    // Meaning column deliberately oversized (never stretched):
    // long entries always keep horizontal scroll available.
    QVERIFY(!table->horizontalHeader()->stretchLastSection());
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    input->setText(QStringLiteral("あ"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(table->model()->rowCount() > 200, 30000);
    QVERIFY(table->horizontalHeader()->sectionSize(2) > table->viewport()->width());
    QVERIFY(qobject_cast<DictionaryTableDelegate *>(table->itemDelegate()) != nullptr);
}

void TestMainWindow::dictionaryTableDefaultsScrollPerPixel() {
    // Bare construction strips smoothly too (the dialog's Words
    // table never had the .ui properties — the modes live in the
    // class now).
    DictionaryTable table;
    QCOMPARE(table.verticalScrollMode(), QAbstractItemView::ScrollPerPixel);
    QCOMPARE(table.horizontalScrollMode(), QAbstractItemView::ScrollPerPixel);
}

void TestMainWindow::dictionaryRowsMaterializeOnDemand() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<QTableView *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    // Rows exist for every hit at once (indices, not contents);
    // contents materialize per row on first paint. A narrow query
    // proves both: exact row count immediately, real text on
    // request.
    input->setText(QStringLiteral("日本語能力試験"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    const QString heading = model->index(0, 0).data(Qt::DisplayRole).toString();
    QVERIFY2(!heading.isEmpty(), "first row never materialized");
    // Structured senses resolve to abbreviated labels (no entity
    // refs) on the first rows.
    bool anyPos = false;
    for (int r = 0; r < qMin(5, model->rowCount()); ++r) {
        const QVector<SenseDisplay> senses =
            model->index(r, 2).data(DictionaryModel::SensesRole).value<QVector<SenseDisplay>>();
        for (const SenseDisplay &sense : senses) {
            for (const QString &label : sense.pos) {
                QVERIFY(!label.contains(QLatin1Char('&')) && !label.contains(QLatin1Char(';')));
                anyPos = true;
            }
        }
        if (anyPos) {
            break;
        }
    }
    QVERIFY2(anyPos, "no pos label in first rows");
    // あ matches tens of thousands: every row exists at once
    // (indices are free), contents still materialize per row.
    input->setText(QStringLiteral("あ"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 200, 30000);
    const QString lastHeading =
        model->index(model->rowCount() - 1, 0).data(Qt::DisplayRole).toString();
    QVERIFY2(!lastHeading.isEmpty(), "last row never materialized");
}

void TestMainWindow::dictionaryHeadingDragSelectsAndCopies() {
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
    // I-beam over heading cells, arrow elsewhere.
    QTest::mouseMove(table->viewport(), cell.center());
    QCOMPARE(table->viewport()->cursor().shape(), Qt::IBeamCursor);
    // Drag across the whole cell: the full heading selects, with
    // no editor widget involved and nothing item-selected.
    QTest::mousePress(table->viewport(), Qt::LeftButton, {}, cell.topLeft() + QPoint(2, 2));
    QTest::mouseMove(table->viewport(), cell.topRight() + QPoint(-2, 2));
    QTest::mouseRelease(table->viewport(), Qt::LeftButton, {}, cell.topRight() + QPoint(-2, 2));
    QVERIFY(table->hasTextSelection());
    QCOMPARE(table->selectedText(), QStringLiteral("食べる"));
    // Press-drag also selects the row (whole-row click feedback);
    // still no editor widget.
    QVERIFY(table->selectionModel()->isRowSelected(0, QModelIndex()));
    QVERIFY(table->viewport()->findChild<QLineEdit *>() == nullptr);
    // Ctrl+C lands the selection on the clipboard.
    QTest::keyClick(table, Qt::Key_C, Qt::ControlModifier);
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("食べる"));
    // Escape clears; the meaning column never selects.
    QTest::keyClick(table, Qt::Key_Escape);
    QVERIFY(!table->hasTextSelection());
    const QRect meaning = table->visualRect(model->index(0, 2));
    QTest::mouseMove(table->viewport(), meaning.center());
    QCOMPARE(table->viewport()->cursor().shape(), Qt::ArrowCursor);
    QTest::mousePress(table->viewport(), Qt::LeftButton, {}, meaning.center());
    QTest::mouseMove(table->viewport(), meaning.topRight());
    QTest::mouseRelease(table->viewport(), Qt::LeftButton, {}, meaning.topRight());
    QVERIFY(!table->hasTextSelection());
    // A new search clears a live selection (rows change identity).
    QTest::mousePress(table->viewport(), Qt::LeftButton, {}, cell.topLeft() + QPoint(2, 2));
    QTest::mouseMove(table->viewport(), cell.topRight() + QPoint(-2, 2));
    QTest::mouseRelease(table->viewport(), Qt::LeftButton, {}, cell.topRight() + QPoint(-2, 2));
    QVERIFY(table->hasTextSelection());
    input->setText(QString());
    QTRY_VERIFY_WITH_TIMEOUT(!table->hasTextSelection(), 5000);
}

void TestMainWindow::dictionaryQueryResetsScrollToTop() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    auto *bar = table->verticalScrollBar();
    // Two multi-page result sets (both kana queries overflow).
    input->setText(QStringLiteral("あ"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 200, 30000);
    QVERIFY(bar->maximum() > 0);
    bar->setValue(bar->maximum());
    QCOMPARE(bar->value(), bar->maximum());
    // A new query jumps back to the top (not a glide: instant).
    input->setText(QStringLiteral("い"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 200, 30000);
    QVERIFY(bar->maximum() > 0);
    QTRY_VERIFY_WITH_TIMEOUT(bar->value() == 0, 5000);
}

void TestMainWindow::dictionaryTableWheelStepsImmediately() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    input->setText(QStringLiteral("あ"));
    QTest::keyClick(input, Qt::Key_Return);
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 200, 30000);
    auto *bar = table->verticalScrollBar();
    QVERIFY(bar->maximum() > 120);
    QCOMPARE(bar->value(), 0);
    // One notch down: a single immediate detent step, applied
    // synchronously (no 180ms flight to wait out).
    const QPoint center = table->viewport()->rect().center();
    QWheelEvent down(center, table->viewport()->mapToGlobal(center), QPoint(), QPoint(0, -120),
                     Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(table->viewport(), &down);
    QApplication::processEvents();
    QVERIFY2(bar->value() > 0, "notch did not scroll");
    QVERIFY2(bar->value() <= 120, "one notch overshot");
}

void TestMainWindow::dictionaryHoverHighlightsRow() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    input->setText(QStringLiteral("あ"));
    QTest::keyClick(input, Qt::Key_Return);
    auto *model = table->model();
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 200, 30000);
    // Hover the first row: its corner pixel takes the wash while a
    // far row keeps the base fill — and nothing gets selected.
    QTest::mouseMove(table->viewport(), table->visualRect(model->index(0, 0)).center());
    const auto corner = [&](int row) {
        const QRect cell = table->visualRect(model->index(row, 0));
        return table->viewport()->grab().toImage().pixelColor(cell.topLeft() + QPoint(3, 3));
    };
    QTRY_VERIFY_WITH_TIMEOUT(corner(0) != corner(5), 5000);
    // Translucent, not solid: the wash is an alpha fill over the
    // base, never the raw highlight color (opaque style hover
    // buried text — see `washFor`).
    QVERIFY(corner(0) != table->palette().color(QPalette::Highlight));
    QVERIFY(table->selectionModel()->selectedIndexes().isEmpty());
}

void TestMainWindow::dictionaryTypingRedirectsToSearch() {
    // Type-to-search: printable keys pressed with the table
    // focused focus the query field and land there converted —
    // no keystroke lost to the table's own keyboard search.
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<QTableView *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    input->clear();
    table->setFocus(Qt::OtherFocusReason);
    QTRY_VERIFY_WITH_TIMEOUT(QApplication::focusWidget() == table, 5000);
    // One keystroke at a time to the focused widget, like real
    // input routing (QTest would otherwise bypass focus).
    QTest::keyClick(QApplication::focusWidget(), 's');
    QCOMPARE(QApplication::focusWidget(), input);
    QTest::keyClick(QApplication::focusWidget(), 'u');
    QCOMPARE(input->text(), QStringLiteral("す"));
    // Arrows still belong to the focused widget (navigation, not
    // input): the field keeps them to itself once focused.
    QTest::keyClick(input, Qt::Key_Left);
    QCOMPARE(input->cursorPosition(), 0);
}
