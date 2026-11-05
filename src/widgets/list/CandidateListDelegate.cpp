#include "widgets/list/CandidateListDelegate.h"

#include "core/palette/MaterialPalette.h"

#include <QColor>
#include <QFontMetrics>
#include <QListView>
#include <QModelIndex>
#include <QObject>
#include <QPainter>
#include <QPalette>
#include <QSize>
#include <QString>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <Qt>
#include <QtGlobal>

#include <utility>

CandidateListDelegate::CandidateListDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void CandidateListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const {
    const QString text = index.data(Qt::DisplayRole).toString();
    const bool selected = (option.state & QStyle::State_Selected) != 0;
    const bool hovered = (option.state & QStyle::State_MouseOver) != 0;
    // Window-backgrounded: resolve from the live palette.
    const MaterialPalette theme = MaterialPalette::fromPalette(option.palette);
    QColor background = theme.window();
    QColor foreground = theme.windowText();
    // Hover without selection: dark themes invert; light themes
    // lay the table's hover wash instead (no invert — dark text on
    // a dark wash would flash). Selection keeps the classic swap
    // in both themes.
    const bool wash = hovered && !selected && !theme.isDark();
    if (!wash && (selected || hovered)) {
        std::swap(background, foreground);
    }
    painter->fillRect(option.rect, background);
    if (wash) {
        painter->fillRect(option.rect, theme.hoverWash());
    }
    if (!text.isEmpty()) {
        painter->setPen(foreground);
        painter->setFont(option.font);
        painter->drawText(
            option.rect, Qt::AlignCenter,
            painter->fontMetrics().elidedText(text, Qt::ElideRight, option.rect.width()));
    }
}

QSize CandidateListDelegate::sizeHint(const QStyleOptionViewItem &option,
                                      const QModelIndex & /*index*/) const {
    // The view's grid rules item geometry (it takes the max over
    // grid and hint), so report it back exactly: anything else
    // would overflow or underfill the cells. The delegate is always
    // parented to its list (see wireUi).
    if (const auto *view = qobject_cast<const QListView *>(parent())) {
        const QSize grid = view->gridSize();
        if (!grid.isEmpty()) {
            return grid;
        }
    }
    const int side = qRound(QFontMetrics(option.font).height() * cellSideFactor);
    return {side, side};
}
