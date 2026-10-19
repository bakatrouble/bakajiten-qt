#pragma once

#include <QColor>

class QStyleOptionViewItem;
class QModelIndex;
class QWidget;
class DictionaryTable;

/// Per-cell pens and washes for the results table: hover wash,
/// deinflected-row tint, kana-only heading fade, query-match ink.
/// Filled manually (never via the brush — styles composite that
/// path their own way). Transparent means "no adjustment".
const DictionaryTable *tableFor(const QWidget *widget);
QColor washFor(const QStyleOptionViewItem &option, const QModelIndex &index);
QColor tintFor(const QStyleOptionViewItem &option, const QModelIndex &index);
QColor headingPenFor(const QStyleOptionViewItem &option, const QModelIndex &index);
QColor matchPenFor(const QStyleOptionViewItem &option);
