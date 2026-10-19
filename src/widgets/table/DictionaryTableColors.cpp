#include "widgets/table/DictionaryTableColors.h"

#include "core/dictionary/DictionaryModel.h"
#include "widgets/table/DictionaryTable.h"

#include "core/palette/MaterialPalette.h"

#include <QColor>
#include <QObject>
#include <QPalette>
#include <QStyleOptionViewItem>
#include <QWidget>
#include <Qt>

/// View behind a style option (the widget is the viewport, the table
/// its parent).
const DictionaryTable *tableFor(const QWidget *widget) {
    const auto *table = qobject_cast<const DictionaryTable *>(widget);
    if (table == nullptr && widget != nullptr) {
        table = qobject_cast<const DictionaryTable *>(widget->parentWidget());
    }
    return table;
}

/// Hover wash for one cell: translucent highlight on the hovered,
/// unselected row, transparent anywhere else. Filled manually (never
/// via the brush — styles composite that path their own way). Note:
/// the empty case is `Qt::transparent`, never `QColor()` — the
/// default-constructed color is invalid yet reports full alpha,
/// which paints as solid black.
QColor washFor(const QStyleOptionViewItem &option, const QModelIndex &index) {
    if ((option.state & QStyle::State_Selected) != 0) {
        return QColor(Qt::transparent);
    }
    const DictionaryTable *table = tableFor(option.widget);
    if (table == nullptr || table->hoverRow() != index.row() || index.row() < 0) {
        return QColor(Qt::transparent);
    }
    // Soft wash: strong enough to track the row, faint enough to
    // leave text (and the green tag prefix) fully legible.
    return MaterialPalette::fromPalette(option.palette).hoverWash();
}
/// Deinflected-row tint for one cell: translucent green under
/// conjugated-form hits (reason chain present), transparent on
/// direct matches. Same manual fill as the hover wash (never via
/// the brush); painted beneath the wash so hover still reads on
/// tinted rows. Green is the app accent (tags, badges), kept faint
/// enough to leave text fully legible on either theme.
QColor tintFor(const QStyleOptionViewItem &option, const QModelIndex &index) {
    if (!index.data(DictionaryModel::DeinflectedRole).toBool()) {
        return QColor(Qt::transparent);
    }
    return MaterialPalette::fromPalette(option.palette).deinflectedTint();
}

/// Heading pen for one cell: the normal text pen at slight
/// transparency on usually-kana headings, opaque elsewhere
/// (headings only). Copied from zkanji (its KanaOnly note paints
/// such headings in QColor(144, 176, 160)); here as transparency
/// rather than a recolor or background tint, so headings keep
/// their theme ink and drag-copy text stays clean.
QColor headingPenFor(const QStyleOptionViewItem &option, const QModelIndex &index) {
    const bool selected = (option.state & QStyle::State_Selected) != 0;
    const MaterialPalette palette = MaterialPalette::fromPalette(option.palette);
    if (index.column() != 0 || !index.data(DictionaryModel::KanaOnlyRole).toBool()) {
        return palette.text(selected);
    }
    return palette.fadedText(selected);
}
/// Match pen for one cell: the accent green (same Material tones
/// as tags: readable on both themes), opaque in both themes. The
/// text color alone marks the query/candidate match span (no
/// background fill).
QColor matchPenFor(const QStyleOptionViewItem &option) {
    return MaterialPalette::fromPalette(option.palette).accentGreen();
}
