#pragma once

#include "widgets/list/CandidateList.h"
#include "widgets/list/CandidateListDelegate.h"

#include <QSize>
#include <QStyledItemDelegate>

/// Candidate strip exclusive to the handwriting panel: double-size
/// type, content-width cells in a single horizontal row (scroll
/// to reveal). Plain cells (no script tint); hover still inverts.
/// Subclasses CandidateList so hover cards and the dev loader
/// keep working through the base type.
class RecognizerCandidateList : public CandidateList {
    Q_OBJECT
public:
    /// Font scale over the interface font (1.5x).
    static constexpr double fontScaleFactor = 1.5;
    /// Row height factor over the list font height: single line
    /// height, so it follows the font size — glyphs never clipped.
    static constexpr double cellHeightFactor = 1.0;
    /// Horizontal breathing room around the text.
    static constexpr int cellPaddingPx = 16;

    explicit RecognizerCandidateList(QWidget *parent = nullptr);

    /// Re-derive the font and the single-row height.
    void refreshScale() override;
    /// No-op: items size from their own hints, never a grid.
    void refreshGrid() override;

private:
    RecognizerCandidateList(const RecognizerCandidateList &) = delete;
    RecognizerCandidateList &operator=(const RecognizerCandidateList &) = delete;
};

/// Content-sized cells for the recognizer strip: width follows
/// the text, height follows the strip row.
class RecognizerCandidateDelegate : public CandidateListDelegate {
    Q_OBJECT
public:
    explicit RecognizerCandidateDelegate(QObject *parent = nullptr);

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};
