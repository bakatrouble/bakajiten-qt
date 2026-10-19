#include "widgets/text/RunLabel.h"

#include <QFontMetrics>
#include <QTextLine>

void RunLabel::relayout(int width) const {
    if (width == laidWidth) {
        return;
    }
    laidWidth = width;
    layouts.clear();
    paraStarts.clear();
    paraTops.clear();
    paraHeights.clear();
    qreal top = 0;
    int start = 0;
    while (true) {
        const int nl = static_cast<int>(full.indexOf(u'\n', start));
        const QString para = nl < 0 ? full.mid(start) : full.mid(start, nl - start);
        auto layout = std::make_unique<QTextLayout>();
        const qreal height = layoutInto(*layout, para, font(), width);
        paraStarts << start;
        paraTops << top;
        paraHeights << height;
        top += height;
        layouts.push_back(std::move(layout));
        if (nl < 0) {
            break;
        }
        start = nl + 1;
    }
}

qreal RunLabel::layoutInto(QTextLayout &layout, const QString &text, const QFont &font, int width) {
    layout.setText(text);
    layout.setFont(font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);
    return placeLines(layout, width);
}

qreal RunLabel::placeLines(QTextLayout &layout, int width) {
    layout.clearLayout();
    layout.beginLayout();
    qreal height = 0;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(qMax(1, width));
        line.setPosition(QPointF(0, height));
        height += line.height();
    }
    layout.endLayout();
    return height;
}

int RunLabel::runIndexAt(int offset) const {
    int at = 0;
    for (int i = 0; i < runs.size(); ++i) {
        if (offset >= at && offset < at + static_cast<int>(runs[i].text.size())) {
            return i;
        }
        at += static_cast<int>(runs[i].text.size());
    }
    return -1;
}

int RunLabel::paragraphAt(qreal y) const {
    for (int p = 0; p < paraTops.size(); ++p) {
        if (y >= paraTops[p] && y < paraTops[p] + paraHeights[p]) {
            return p;
        }
    }
    return -1;
}

int RunLabel::characterAt(const QPoint &pos) const {
    relayout(qMax(1, width()));
    if (pos.x() < 0) {
        return -1;
    }
    const int p = paragraphAt(pos.y());
    if (p < 0) {
        return -1;
    }
    const QTextLayout &layout = *layouts[static_cast<size_t>(p)];
    for (int i = 0; i < layout.lineCount(); ++i) {
        const QTextLine line = layout.lineAt(i);
        // xToCursor is absolute (probe-verified); bounds first so
        // trailing space never clamps into text.
        const qreal top = paraTops[p] + line.y();
        if (pos.y() >= top && pos.y() < top + line.height() && pos.x() >= line.x() &&
            pos.x() <= line.x() + line.width()) {
            return paraStarts[p] + line.xToCursor(pos.x() - line.x(), QTextLine::CursorOnCharacter);
        }
    }
    return -1;
}

int RunLabel::nearestOffset(const QPoint &pos) const {
    relayout(qMax(1, width()));
    int fallback = 0; // end of the last line starting at/above y
    for (int p = 0; p < static_cast<int>(layouts.size()); ++p) {
        const QTextLayout &layout = *layouts[static_cast<size_t>(p)];
        for (int i = 0; i < layout.lineCount(); ++i) {
            const QTextLine line = layout.lineAt(i);
            const qreal top = paraTops[p] + line.y();
            if (pos.y() < top) {
                return fallback; // gap or padding: the line above ends here
            }
            fallback = paraStarts[p] + line.textStart() + line.textLength();
            if (pos.y() < top + line.height()) {
                const qreal x =
                    qBound(line.x(), static_cast<qreal>(pos.x()), line.x() + line.width());
                return paraStarts[p] + line.xToCursor(x - line.x(), QTextLine::CursorOnCharacter);
            }
        }
    }
    return fallback; // below all lines: the very end
}

QString RunLabel::linkAt(const QPoint &pos) const {
    relayout(qMax(1, width()));
    const int offset = characterAt(pos);
    if (offset < 0) {
        return {};
    }
    const int i = runIndexAt(offset);
    if (i < 0 || runs[i].href.isEmpty()) {
        return {};
    }
    return runs[i].href;
}

QList<QPair<QRect, QString>> RunLabel::links() const {
    relayout(qMax(1, width()));
    QList<QPair<QRect, QString>> out;
    int at = 0;
    for (const Run &run : runs) {
        const int end = at + static_cast<int>(run.text.size());
        if (!run.href.isEmpty()) {
            // Runs may start with \n (meta headers), straddling a
            // paragraph boundary: clip against every paragraph.
            for (size_t p = 0; p < layouts.size(); ++p) {
                const int ps = paraStarts[int(p)];
                for (int i = 0; i < layouts[p]->lineCount(); ++i) {
                    const QTextLine line = layouts[p]->lineAt(i);
                    const int from = qMax(at, ps + line.textStart());
                    const int to = qMin(end, ps + line.textStart() + line.textLength());
                    if (to > from) {
                        const int x0 = qRound(line.cursorToX(from - ps));
                        const int x1 = qRound(line.cursorToX(to - ps));
                        out << qMakePair(QRect(x0, qRound(paraTops[int(p)] + line.y()), x1 - x0,
                                               qRound(line.height())),
                                         run.href);
                    }
                }
            }
        }
        at = end;
    }
    return out;
}
