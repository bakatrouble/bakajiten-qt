#include "widgets/table/DictionaryTableDelegate.h"

#include "core/dictionary/DictionaryModel.h"
#include "core/dictionary/MeaningBuilder.h"
#include "util/CellText.h"
#include "widgets/table/DictionaryTable.h"
#include "widgets/table/DictionaryTableColors.h"

#include "core/palette/MaterialPalette.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QObject>
#include <QPainter>
#include <QRect>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QVector>
#include <Qt>
#include <QtGlobal>

#include <utility>

DictionaryTableDelegate::DictionaryTableDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void DictionaryTableDelegate::initStyleOption(QStyleOptionViewItem *option,
                                              const QModelIndex &index) const {
    QStyledItemDelegate::initStyleOption(option, index);
    // No focus frame anywhere in this table: position feedback is
    // the row selection, the hover wash, and the text selection.
    // (Pressing a cell makes it current, and the style would
    // otherwise draw its dotted rect around the cell.)
    option->state &= ~QStyle::State_HasFocus;
    // No style-native hover either: styles paint `State_MouseOver`
    // opaquely (Breeze and friends), which would bury the alpha
    // wash below. Hover feedback is the manual fill in `paint`.
    option->state &= ~QStyle::State_MouseOver;
    QFont font = CellText::cellFont(option->font, index.column());
    if (index.column() == 1) {
        option->palette.setColor(QPalette::Text,
                                 MaterialPalette::fromPalette(option->palette).dimmed());
    }
    option->font = std::move(font);
    // Single line, elided: rows stay uniform (see `DictionaryTable`),
    // overflow never wraps or clips mid-glyph silently.
    option->textElideMode = Qt::ElideRight;
    // Hover feedback is a manual alpha fill at paint time (see
    // `washFor`): brush-based hover composites per style (or not at
    // all), while a direct fillRect is translucent everywhere.
    // Selected rows skip it and keep the solid selection fill.
}

void DictionaryTableDelegate::paintBackground(QPainter *painter, const QStyleOptionViewItem &option,
                                              const QModelIndex &index) const {
    // Background exactly as the base would paint it — through the
    // style directly, because QStyledItemDelegate::paint re-runs
    // initStyleOption internally and would repopulate cleared text.
    QStyleOptionViewItem background(option);
    initStyleOption(&background, index);
    background.text.clear();
    background.icon = QIcon();
    background.features &= ~QStyleOptionViewItem::HasDisplay;
    QStyle *style =
        background.widget != nullptr ? background.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &background, painter, background.widget);
    // Deinflected tint below the hover wash (both SourceOver alpha
    // fills, whatever the style does with brushes).
    if (const QColor tint = tintFor(option, index); tint.alpha() > 0) {
        painter->fillRect(option.rect, tint);
    }
    // Manual alpha wash on top: the painter composites it
    // (SourceOver) whatever the style does with brushes.
    if (const QColor wash = washFor(option, index); wash.alpha() > 0) {
        painter->fillRect(option.rect, wash);
    }
}

void DictionaryTableDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const {
    if (index.column() == 2) {
        paintMeaning(painter, option, index);
        return;
    }
    // Heading/reading: background above, then our own text so
    // selection offsets match drawn glyphs exactly.
    paintBackground(painter, option, index);

    painter->save();
    const QString full = index.data(Qt::DisplayRole).toString();
    const QRect rect = CellText::textRect(option.rect);
    const QFontMetrics metrics(option.font);
    const QString visible = CellText::visibleText(metrics, full, rect.width());
    const bool selected = (option.state & QStyle::State_Selected) != 0;
    painter->setFont(option.font);
    painter->setPen(headingPenFor(option, index));
    painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, visible);
    if (const DictionaryTable *table = tableFor(option.widget);
        table != nullptr && table->hasTextSelection()) {
        const DictionaryTable::TextSelection sel = table->textSelection();
        if (sel.row == index.row() && sel.column == index.column() && sel.length > 0) {
            const int from = qBound(0, sel.start, static_cast<int>(visible.size()));
            const int to = qBound(0, sel.start + sel.length, static_cast<int>(visible.size()));
            if (to > from) {
                const int x0 = rect.left() + metrics.horizontalAdvance(visible.left(from));
                const int x1 = rect.left() + metrics.horizontalAdvance(visible.left(to));
                const QRect span(x0, rect.top(), x1 - x0, rect.height());
                // Inverted pair on selected rows (the row fill is
                // already Highlight): two bright colors stack
                // invisibly otherwise.
                const MaterialPalette palette = MaterialPalette::fromPalette(option.palette);
                painter->fillRect(span, palette.selectionFill(selected));
                painter->setClipRect(span);
                painter->setPen(palette.selectionInk(selected));
                painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                                  visible);
            }
        }
    }
    if (!selected) {
        // Query/candidate match: accent text on the first
        // occurrence, no background (the text color alone marks
        // the match). Selected rows skip it — the whole row is
        // already inverted.
        const QString needle = index.data(DictionaryModel::MatchRole).toString();
        const int at = needle.isEmpty() ? -1 : static_cast<int>(visible.indexOf(needle));
        const int to =
            at < 0 ? at
                   : qMin(at + static_cast<int>(needle.size()), static_cast<int>(visible.size()));
        if (to > at) {
            const int x0 = rect.left() + metrics.horizontalAdvance(visible.left(at));
            const int x1 = rect.left() + metrics.horizontalAdvance(visible.left(to));
            const QRect span(x0, rect.top(), x1 - x0, rect.height());
            painter->setClipRect(span);
            painter->setPen(matchPenFor(option));
            painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, visible);
        }
    }
    painter->restore();
}

void DictionaryTableDelegate::paintMeaning(QPainter *painter, const QStyleOptionViewItem &option,
                                           const QModelIndex &index) const {
    // zkanji-style runs from the builder (numbered senses, small
    // tag runs, gloss), laid left to right on one shared baseline
    // and clipped at the cell edge (no wrap, rows stay uniform).
    paintBackground(painter, option, index);
    const bool selected = (option.state & QStyle::State_Selected) != 0;
    const QVector<MeaningBuilder::Run> runs = MeaningBuilder::build(
        index.data(DictionaryModel::SensesRole).value<QVector<SenseDisplay>>(), option.font,
        index.data(DictionaryModel::InflectionRole).toString(), option.palette, selected);
    if (runs.isEmpty()) {
        return;
    }
    painter->save();
    painter->setClipRect(option.rect);
    // Baseline of the base-size runs, centered exactly as before
    // (`AlignVCenter` on the base font lands here); every run —
    // including the smaller tag fonts — draws on it.
    const QFontMetrics baseMetrics(option.font);
    const int baseline =
        option.rect.center().y() + (baseMetrics.ascent() - baseMetrics.descent()) / 2;
    const QVector<MeaningBuilder::PlacedRun> placed = MeaningBuilder::layoutRuns(
        runs, {option.rect.left() + CellText::margin, option.rect.right()});
    for (int i = 0; i < runs.size() && i < placed.size(); ++i) {
        const MeaningBuilder::Run &run = runs[i];
        painter->setFont(run.font);
        painter->setPen(run.pen);
        painter->drawText(placed[i].x, baseline, placed[i].shown);
    }
    painter->restore();
}
