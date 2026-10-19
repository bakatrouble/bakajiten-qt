#include "widgets/text/RubyLabel.h"

#include "core/palette/MaterialPalette.h"

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <numeric>

namespace {
constexpr int rubyGap = 2;
constexpr int selectionNone = -1;
} // namespace

RubyLabel::RubyLabel(QWidget *parent) : QWidget(parent) {
    setCursor(Qt::IBeamCursor);
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void RubyLabel::setSpans(const QVector<Span> &spans) {
    this->spans = spans;
    full.clear();
    for (const Span &span : spans) {
        full += span.base;
    }
    selAnchor = selectionNone;
    selPos = selectionNone;
    updateGeometry();
    update();
}

QString RubyLabel::text() const { return full; }

int RubyLabel::shownSize() const {
    int total = 0;
    for (const Span &span : shownSpans()) {
        total += static_cast<int>(span.base.size());
    }
    return total;
}

QFont RubyLabel::baseFont() const {
    QFont font = this->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() + 2.0);
    return font;
}

QFont RubyLabel::rubyFont() const {
    QFont font = this->font();
    font.setPointSizeF(qMax(6.0, font.pointSizeF() - 2.0));
    return font;
}

QVector<int> RubyLabel::columnWidths() const { return columnsFor(spans); }

QString RubyLabel::selectedText() const {
    if (selAnchor < 0 || selPos < 0 || selAnchor == selPos) {
        return {};
    }
    // Offsets live in shown display space; each span maps back to
    // its keb segment. Verbatim spans (display == keb) slice
    // per-character; converted spans (romaji display) have no
    // character alignment, so any overlap takes the whole segment.
    // The focus edge covers its glyph (a drag onto a glyph includes
    // it — cursor-exclusive ends would stop every drag one short).
    const QVector<Span> spans = shownSpans();
    const int total = shownSize();
    const int from = qBound(0, qMin(selAnchor, selPos), total);
    const int to = qBound(0, qMax(selAnchor, selPos) + 1, total);
    QString out;
    int at = 0;
    for (const Span &span : spans) {
        const QString keb = span.keb.isEmpty() ? span.base : span.keb;
        const int overlapFrom = qMax(from, at);
        const int overlapTo = qMin(to, at + static_cast<int>(span.base.size()));
        if (overlapTo > overlapFrom) {
            out += (keb == span.base) ? keb.mid(overlapFrom - at, overlapTo - overlapFrom) : keb;
        }
        at += static_cast<int>(span.base.size());
    }
    return out;
}

void RubyLabel::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // Double-click selects the whole written form (keb), not
        // the display text.
        selAnchor = 0;
        selPos = shownSize();
        update();
        // Accept: an ignored double-click falls back to a press at
        // the click point (outside the text here), which would
        // clear the just-made selection in mousePressEvent.
        event->accept();
        // Kanji dialog hook (the opener filters kana through kanji
        // info): surrogate-joined like the hover path.
        const int offset = characterAt(event->pos());
        const QString shown = text();
        if (offset >= 0 && offset < shown.size()) {
            emit kanjiDoubleClicked(shown.mid(offset, shown.at(offset).isHighSurrogate() ? 2 : 1));
        }
    }
}

QSize RubyLabel::sizeHint() const {
    int width = 0;
    for (const int w : columnWidths()) {
        width += w;
    }
    const QFontMetrics base(baseFont());
    const QFontMetrics ruby(rubyFont());
    return QSize(width, ruby.height() + rubyGap + base.height());
}

void RubyLabel::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    const MaterialPalette theme = MaterialPalette::fromPalette(palette());
    const QFont baseF = baseFont();
    const QFont rubyF = rubyFont();
    const QFontMetrics base(baseF);
    const QFontMetrics ruby(rubyF);
    const QVector<Span> shown = shownSpans();
    const QVector<int> widths = columnsFor(shown);
    const int rubyBaseline = ruby.ascent();
    const int baseBaseline = ruby.height() + rubyGap + base.ascent();
    const bool selected = selAnchor >= 0 && selPos >= 0 && selAnchor != selPos;
    // Focus edge covers its glyph (mirrors selectedText).
    const int selFrom = qMin(selAnchor, selPos);
    const int selTo = qMax(selAnchor, selPos) + 1;
    int x = 0;
    int offset = 0;
    for (int s = 0; s < shown.size(); ++s) {
        const Span &span = shown[s];
        if (!span.ruby.isEmpty()) {
            painter.setFont(rubyF);
            painter.setPen(theme.dimmed());
            const int rubyW = ruby.horizontalAdvance(span.ruby);
            painter.drawText(x + (widths[s] - rubyW) / 2, rubyBaseline, span.ruby);
        }
        const int baseW = base.horizontalAdvance(span.base);
        const int baseX = x + (widths[s] - baseW) / 2;
        if (selected && offset + span.base.size() > selFrom && offset < selTo) {
            int selX = baseX;
            for (int i = 0; i < qMax(0, selFrom - offset);) {
                selX += charAdvance(base, span.base, i);
            }
            int selW = 0;
            for (int i = qMax(0, selFrom - offset); i < qMin(span.base.size(), selTo - offset);) {
                selW += charAdvance(base, span.base, i);
            }
            painter.fillRect(selX, baseBaseline - base.ascent(), selW, base.height(),
                             palette().color(QPalette::Highlight));
        }
        painter.setFont(baseF);
        painter.setPen(selected && offset + span.base.size() > selFrom && offset < selTo
                           ? palette().color(QPalette::HighlightedText)
                           : theme.text());
        painter.drawText(baseX, baseBaseline, span.base);
        x += widths[s];
        offset += static_cast<int>(span.base.size());
    }
}

void RubyLabel::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        setFocus(Qt::MouseFocusReason);
        selAnchor = characterAt(event->pos());
        selPos = selAnchor;
        update();
        // Consumed (see RunLabel::mousePressEvent): the view's span
        // tracker must see each gesture exactly once.
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void RubyLabel::mouseMoveEvent(QMouseEvent *event) {
    if ((event->buttons() & Qt::LeftButton) != 0) {
        const int offset = characterAt(event->pos());
        if (offset >= 0) {
            selPos = offset;
            update();
        }
        // Same single-delivery rule as the press handler.
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void RubyLabel::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void RubyLabel::keyPressEvent(QKeyEvent *event) {
    if (event->matches(QKeySequence::Copy) && !selectedText().isEmpty()) {
        QApplication::clipboard()->setText(selectedText());
        return;
    }
    QWidget::keyPressEvent(event);
}

void RubyLabel::changeEvent(QEvent *event) {
    if (event->type() == QEvent::FontChange) {
        updateGeometry();
        update();
    }
    QWidget::changeEvent(event);
}
