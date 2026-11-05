#include "widgets/list/RecognizerCandidateList.h"

#include <QApplication>
#include <QFont>
#include <QFontMetricsF>
#include <QFrame>
#include <QModelIndex>
#include <QSize>
#include <QString>
#include <QStyleOptionViewItem>
#include <QWidget>
#include <Qt>

RecognizerCandidateList::RecognizerCandidateList(QWidget *parent) : CandidateList(parent) {
    // Decouple from the shared square strip: free per-item sizes,
    // no grid, no frame (the viewport is the list box exactly),
    // own delegate, single scrolling row.
    delete itemDelegate();
    setItemDelegate(new RecognizerCandidateDelegate(this));
    setUniformItemSizes(false);
    setGridSize(QSize());
    setFrameStyle(QFrame::NoFrame);
    setMinimumHeight(20);
    setMaximumHeight(20);
}

void RecognizerCandidateList::refreshScale() {
    // Interface font at the current scale (never the list's own:
    // it is explicit after the first call, so re-deriving from it
    // would compound 2x into 4x on every live font change).
    QFont listFont = QApplication::font();
    if (listFont.pointSizeF() > 0.0) {
        listFont.setPointSizeF(listFont.pointSizeF() * fontScaleFactor);
    }
    setFont(listFont);
    const int row = qRound(QFontMetricsF(listFont).height() * cellHeightFactor);
    setMinimumHeight(row);
    setMaximumHeight(row);
}

void RecognizerCandidateList::refreshGrid() {
    // No grid: items size from their own hints in one row.
}

RecognizerCandidateDelegate::RecognizerCandidateDelegate(QObject *parent)
    : CandidateListDelegate(parent) {}

QSize RecognizerCandidateDelegate::sizeHint(const QStyleOptionViewItem &option,
                                            const QModelIndex &index) const {
    const QString text = index.data(Qt::DisplayRole).toString();
    const QFontMetricsF metrics(option.font);
    const int row = qRound(metrics.height() * RecognizerCandidateList::cellHeightFactor);
    const int width =
        qRound(metrics.horizontalAdvance(text)) + RecognizerCandidateList::cellPaddingPx;
    return {width, row};
}
