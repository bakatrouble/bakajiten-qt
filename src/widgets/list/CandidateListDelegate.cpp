#include "widgets/list/CandidateListDelegate.h"

#include "core/palette/MaterialPalette.h"
#include "recognizer.hpp"

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

CandidateListDelegate::Script CandidateListDelegate::scriptOf(const QString &text) {
    // First-character ranges live in Rust (same traces); tones
    // resolve trackside below.
    switch (recognizer::script_of(text.toStdString())) {
    case recognizer::Script::kHiragana:
        return Script::Hiragana;
    case recognizer::Script::kKatakana:
        return Script::Katakana;
    default:
        return Script::Other;
    }
}

void CandidateListDelegate::cellColors(Script script, bool inverted, const QPalette &palette,
                                       QColor &background, QColor &foreground) {
    // Kana scripts only: everything else resolves Window/WindowText
    // from the live palette at the paint site.
    const MaterialPalette theme = MaterialPalette::fromPalette(palette);
    const MaterialPalette::TonalSpot spot = theme.kanaSpot(script == Script::Katakana);
    background = spot.container;
    foreground = spot.onContainer;
    if (inverted) {
        std::swap(background, foreground);
    }
}

void CandidateListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const {
    const QString text = index.data(Qt::DisplayRole).toString();
    const Script script = scriptOf(text);
    const bool active = (option.state & (QStyle::State_MouseOver | QStyle::State_Selected)) != 0;
    QColor background;
    QColor foreground;
    if (script == Script::Other) {
        // Window-backgrounded: resolve from the live palette.
        const MaterialPalette theme = MaterialPalette::fromPalette(option.palette);
        background = theme.window();
        foreground = theme.windowText();
        if (active) {
            std::swap(background, foreground);
        }
    } else {
        cellColors(script, active, option.palette, background, foreground);
    }
    painter->fillRect(option.rect, background);
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
