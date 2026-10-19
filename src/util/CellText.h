#pragma once

#include <QFont>
#include <QFontMetrics>
#include <QRect>
#include <QString>

/// Cell-text geometry shared by the results delegate (paint) and the
/// table (drag-select hit testing): one source so selection math and
/// drawn glyphs always agree. Cells are single-line, left-aligned,
/// elided on the right; heading is bold and larger. Meaning runs
/// themselves live in `MeaningBuilder` (runs plus resolved pens).
namespace CellText {
inline constexpr int margin = 4;

inline QFont cellFont(QFont base, int column) {
    if (column == 0) {
        base.setBold(true);
        base.setPointSizeF(base.pointSizeF() + 2.0);
    }
    return base;
}

inline QRect textRect(const QRect &cell) { return cell.adjusted(margin, 0, -margin, 0); }

inline QString visibleText(const QFontMetrics &metrics, const QString &full, int width) {
    return metrics.elidedText(full, Qt::ElideRight, width);
}

/// UTF-16 offset into `visible` at horizontal pixel `x` (relative to
/// the text left edge), clamped to [0, size]. Linear scan — cell
/// strings are short.
inline int indexAtX(const QFontMetrics &metrics, const QString &visible, int x) {
    if (x <= 0) {
        return 0;
    }
    for (int i = 1; i <= visible.size(); ++i) {
        if (metrics.horizontalAdvance(visible.left(i)) >= x) {
            return i - 1;
        }
    }
    return static_cast<int>(visible.size());
}
} // namespace CellText
