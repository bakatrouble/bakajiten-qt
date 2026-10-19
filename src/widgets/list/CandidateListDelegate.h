#pragma once

#include <QColor>
#include <QStyledItemDelegate>

#include <cstdint>

/// Candidate cells, fully custom: strictly square, painted on the
/// window background, script-colored (katakana green, hiragana blue
/// via the Material generator), inverted on hover/selection. The
/// view's grid keeps cells square; the delegate fills option.rect
/// exactly, so item and cell geometry always agree.
class CandidateListDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    /// Script of a candidate's leading character (everything else —
    /// kanji, latin, the empty-hint row — is Other).
    enum class Script : std::uint8_t { Other, Hiragana, Katakana };

    /// Cell side factor over the list font height (shared with the
    /// strip sizing in Candidates.cpp).
    static constexpr double cellSideFactor = 1.4;

    explicit CandidateListDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    static Script scriptOf(const QString &text);
    /// Kana cell colors (pure: unit-testable). Inverted swaps
    /// background and foreground (hover/selected). Other scripts
    /// resolve Window/WindowText at the paint site instead.
    static void cellColors(Script script, bool inverted, const QPalette &palette,
                           QColor &background, QColor &foreground);
};
