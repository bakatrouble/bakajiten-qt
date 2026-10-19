#include "widgets/text/RubyLabel.h"

#include <QFontMetrics>

#include <numeric>

QVector<int> RubyLabel::columnsFor(const QVector<Span> &list) const {
    const QFontMetrics base(baseFont());
    const QFontMetrics ruby(rubyFont());
    QVector<int> widths;
    for (const Span &span : list) {
        widths << qMax(base.horizontalAdvance(span.base), ruby.horizontalAdvance(span.ruby));
    }
    return widths;
}

/// Spans fitted to the current width: trailing spans dropped under
/// overflow, single-span overflow elided. Paint, hit-testing, and
/// selection share this (offsets live in shown space).
QVector<RubyLabel::Span> RubyLabel::shownSpans() const {
    if (width() <= 0) {
        return spans;
    }
    QVector<Span> shown = spans;
    const auto total = [&] {
        int sum = 0;
        for (const int w : columnsFor(shown)) {
            sum += w;
        }
        return sum;
    };
    while (shown.size() > 1 && total() > width()) {
        shown.removeLast();
    }
    if (!shown.isEmpty() && total() > width()) {
        const QFontMetrics baseMetrics(baseFont());
        const QFontMetrics rubyMetrics(rubyFont());
        Span &last = shown.last();
        last.base = baseMetrics.elidedText(last.base, Qt::ElideRight, width());
        if (!last.ruby.isEmpty()) {
            last.ruby = rubyMetrics.elidedText(last.ruby, Qt::ElideRight, width());
        }
    }
    return shown;
}

int RubyLabel::spanIndexAt(int x) const {
    int at = 0;
    const QVector<int> widths = columnsFor(shownSpans());
    for (int i = 0; i < widths.size(); ++i) {
        if (x >= at && x < at + widths[i]) {
            return i;
        }
        at += widths[i];
    }
    return -1;
}

/// Advance of one user-perceived character (surrogate pairs move as
/// one, matching the tooltip half of the hover path).
int RubyLabel::charAdvance(const QFontMetrics &metrics, const QString &text, int &i) {
    const int size = (i + 1 < text.size() && text.at(i).isHighSurrogate()) ? 2 : 1;
    const int advance = metrics.horizontalAdvance(text.mid(i, size));
    i += size;
    return advance;
}

int RubyLabel::characterAt(const QPoint &pos) const {
    if (pos.y() < 0 || pos.y() >= height()) {
        return -1;
    }
    const QVector<Span> shown = shownSpans();
    const int span = spanIndexAt(pos.x());
    if (span < 0 || span >= shown.size()) {
        return -1;
    }
    int base = 0;
    for (int i = 0; i < span; ++i) {
        base += static_cast<int>(shown[i].base.size());
    }
    const QVector<int> widths = columnsFor(shown);
    const QFontMetrics metrics(baseFont());
    const int columnX = pos.x() - std::accumulate(widths.begin(), widths.begin() + span, 0);
    const int textW = metrics.horizontalAdvance(shown[span].base);
    int charX = columnX - (widths[span] - textW) / 2;
    int offset = base;
    for (int i = 0; i < shown[span].base.size();) {
        const int advance = charAdvance(metrics, shown[span].base, i);
        if (charX < advance) {
            break;
        }
        charX -= advance;
        offset = base + i;
    }
    // Cursor semantics (0..size, one-past-the-end included):
    // selection edges must reach the end of the span. Glyph
    // consumers (hover, dialog hook) guard `offset < size`
    // themselves — mid/at on the end offset would trip QString
    // asserts.
    const int size = static_cast<int>(shown[span].base.size());
    if (size == 0) {
        return -1;
    }
    return qBound(base, offset, base + size);
}

QRect RubyLabel::characterRect(int offset) const {
    // Shown space (dropped spans have no rect).
    const QVector<Span> shown = shownSpans();
    int total = 0;
    for (const Span &span : shown) {
        total += static_cast<int>(span.base.size());
    }
    if (offset < 0 || offset >= total) {
        return {};
    }
    int base = 0;
    int x = 0;
    const QVector<int> widths = columnsFor(shown);
    const QFontMetrics metrics(baseFont());
    for (int s = 0; s < shown.size(); ++s) {
        if (offset >= base && offset < base + static_cast<int>(shown[s].base.size())) {
            const int textW = metrics.horizontalAdvance(shown[s].base);
            int charX = x + (widths[s] - textW) / 2;
            for (int i = 0; i < offset - base;) {
                charX += charAdvance(metrics, shown[s].base, i);
            }
            int i = offset - base;
            const int width = charAdvance(metrics, shown[s].base, i);
            return QRect(charX, 0, qMax(1, width), height());
        }
        base += static_cast<int>(shown[s].base.size());
        x += widths[s];
    }
    return {};
}
