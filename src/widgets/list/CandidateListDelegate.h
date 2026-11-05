#pragma once

#include <QStyledItemDelegate>

/// Candidate cells, fully custom: strictly square, painted on the
/// window background, inverted on hover/selection. The view's grid
/// keeps cells square; the delegate fills option.rect exactly, so
/// item and cell geometry always agree.
class CandidateListDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    /// Cell side factor over the list font height (shared with the
    /// strip sizing in Candidates.cpp).
    static constexpr double cellSideFactor = 1.4;

    explicit CandidateListDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};
