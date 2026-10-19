#include "widgets/table/DictionaryTable.h"

#include "core/dictionary/DictionaryModel.h"
#include "core/dictionary/MeaningBuilder.h"
#include "util/CellText.h"

#include <QChar>
#include <QFontMetrics>
#include <QPoint>
#include <QString>
#include <QVector>
#include <Qt>

void DictionaryTable::updateCursor(const QPoint &pos) {
    const int column = columnAt(pos.x());
    const int row = rowAt(pos.y());
    if ((column == 0 || column == 1) && row >= 0) {
        viewport()->setCursor(Qt::IBeamCursor);
    } else {
        viewport()->unsetCursor();
    }
}

void DictionaryTable::onScrolled() {
    if (!mouseInside || !viewport()->rect().contains(lastMouse)) {
        return;
    }
    setHoverRow(rowAt(lastMouse.y()));
    updateCursor(lastMouse);
    updateTagTip(lastMouse);
}

/// Description behind the meaning-column tag run under the cursor
/// (empty anywhere else — gloss text, numbers, other columns).
QString DictionaryTable::kanjiAt(const QPoint &pos) const {
    const int row = rowAt(pos.y());
    const int column = columnAt(pos.x());
    if (row < 0 || model() == nullptr) {
        return {};
    }
    if (column == 0 || column == 1) {
        const QString full = model()->index(row, column).data(Qt::DisplayRole).toString();
        const QFontMetrics metrics(CellText::cellFont(font(), column));
        const int width = CellText::textRect(visualRect(model()->index(row, column))).width();
        const QString visible = CellText::visibleText(metrics, full, width);
        const int left = CellText::textRect(visualRect(model()->index(row, column))).left();
        const int off = CellText::indexAtX(metrics, visible, pos.x() - left);
        return charAtVisible(visible, off);
    }
    if (column == 2) {
        const QVector<SenseDisplay> senses =
            model()->index(row, 2).data(DictionaryModel::SensesRole).value<QVector<SenseDisplay>>();
        const QString inflection =
            model()->index(row, 2).data(DictionaryModel::InflectionRole).toString();
        const QVector<MeaningBuilder::Run> runs =
            MeaningBuilder::build(senses, font(), inflection, palette(), false);
        const QRect cellRect = visualRect(model()->index(row, 2));
        const QVector<MeaningBuilder::PlacedRun> placed = MeaningBuilder::layoutRuns(
            runs, {CellText::textRect(cellRect).left(), cellRect.right()});
        for (int i = 0; i < runs.size() && i < placed.size(); ++i) {
            const QFontMetrics metrics(runs[i].font);
            const int w = metrics.horizontalAdvance(placed[i].shown);
            if (pos.x() >= placed[i].x && pos.x() < placed[i].x + w) {
                const int off = CellText::indexAtX(metrics, placed[i].shown, pos.x() - placed[i].x);
                return charAtVisible(placed[i].shown, off);
            }
        }
    }
    return {};
}

/// One character at a UTF-16 offset (surrogate-aware).
QString DictionaryTable::charAtVisible(const QString &text, int off) {
    if (off < 0 || off >= text.size()) {
        return {};
    }
    const QChar c = text.at(off);
    if (c.isHighSurrogate() && off + 1 < text.size()) {
        return text.mid(off, 2);
    }
    return QString(c);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
