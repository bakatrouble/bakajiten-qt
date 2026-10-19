#include "widgets/table/DictionaryTable.h"

#include "util/CellText.h"

#include <QClipboard>
#include <QFontMetrics>
#include <QItemSelectionModel>
#include <QSignalBlocker>
#include <Qt>
#include <QtGlobal>

QString DictionaryTable::selectedText() const {
    if (!hasTextSelection() || model() == nullptr) {
        return {};
    }
    return model()
        ->index(selection.row, selection.column)
        .data(Qt::DisplayRole)
        .toString()
        .mid(selection.start, selection.length);
}

void DictionaryTable::clearSelections() {
    if (!hasTextSelection() && !selecting &&
        (selectionModel() == nullptr || !selectionModel()->hasSelection())) {
        return;
    }
    selecting = false;
    selection = TextSelection();
    // Current first: the selectionChanged handler below must see
    // the cleared state, not the stale row.
    setCurrentIndex(QModelIndex());
    if (selectionModel() != nullptr) {
        selectionModel()->clear();
    }
    viewport()->update();
}

/// Row selection lands the current index in the last (meaning)
/// column — Qt's selectRow picks it — and the view scrolls the
/// 6000px cell into view, yanking the horizontal scrollbar on
/// every row change (including article open). Nothing reads the
/// current column (only the row), so pin it to the heading column
/// with updates blocked: no scroll, and no signal loop.
void DictionaryTable::pinCurrentToText(const QModelIndex &current) {
    if (current.isValid() && current.column() == 2) {
        const QSignalBlocker block(selectionModel());
        selectionModel()->setCurrentIndex(current.siblingAtColumn(0),
                                          QItemSelectionModel::NoUpdate);
    }
}

void DictionaryTable::pressAt(const QPoint &pos) {
    const int row = rowAt(pos.y());
    const int column = columnAt(pos.x());
    if (row < 0 || (column != 0 && column != 1)) {
        clearSelections();
        return;
    }
    anchor = charAt(row, column, pos.x());
    selecting = true;
    selection.row = row;
    selection.column = column;
    selection.start = anchor;
    selection.length = 0;
    viewport()->update();
}

void DictionaryTable::moveAt(const QPoint &pos) {
    const int row = rowAt(pos.y());
    const int column = columnAt(pos.x());
    if (row != selection.row || column != selection.column) {
        return; // stays inside the origin cell
    }
    const int at = charAt(row, column, pos.x());
    selection.start = qMin(anchor, at);
    selection.length = qAbs(at - anchor);
    viewport()->update();
}

/// UTF-16 offset into one cell's full string at viewport x.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
int DictionaryTable::charAt(int row, int column, int x) const {
    const QModelIndex index = model()->index(row, column);
    const QString full = index.data(Qt::DisplayRole).toString();
    const QFontMetrics metrics(CellText::cellFont(font(), column));
    const int width = CellText::textRect(visualRect(index)).width();
    const QString visible = CellText::visibleText(metrics, full, width);
    const int left = CellText::textRect(visualRect(index)).left();
    return CellText::indexAtX(metrics, visible, x - left);
}

/// I-beam over heading/reading cells, arrow anywhere else.
