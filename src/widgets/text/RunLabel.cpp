#include "widgets/text/RunLabel.h"

#include "core/palette/MaterialPalette.h"
#include "widgets/tooltip/Tooltip.h"

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTextLine>
#include <QTimer>
#include <QToolTip>

namespace {
constexpr int selectionNone = -1;
} // namespace

RunLabel::RunLabel(QWidget *parent) : QWidget(parent) {
    setCursor(Qt::IBeamCursor);
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    tipTimer = new QTimer(this);
    tipTimer->setSingleShot(true);
    tipTimer->setInterval(Tooltip::showDelayMs);
    connect(tipTimer, &QTimer::timeout, this, [this] {
        if (!tipText.isEmpty()) {
            QToolTip::showText(tipAnchor, tipText, this);
        }
    });
}

void RunLabel::setRuns(const QVector<Run> &runs) {
    this->runs = runs;
    full.clear();
    for (const Run &run : runs) {
        full += run.text;
    }
    selAnchor = selectionNone;
    selPos = selectionNone;
    pressOffset = selectionNone;
    hideTip();
    laidWidth = -1;
    updateGeometry();
    update();
}

QString RunLabel::text() const { return full; }

QFont RunLabel::runFont(const Run &run) const {
    QFont font = this->font();
    if (run.small) {
        font.setPointSizeF(qMax(6.0, font.pointSizeF() - 2.0));
    }
    font.setBold(run.bold);
    return font;
}

QColor RunLabel::inkColor(Ink ink) const {
    const MaterialPalette theme = MaterialPalette::fromPalette(palette());
    switch (ink) {
    case Ink::Dim:
        return theme.dimmed();
    case Ink::Green:
        return theme.accentGreen();
    case Ink::Red:
        return theme.warning();
    case Ink::Link:
        return theme.link();
    case Ink::Body:
        break;
    }
    return theme.text();
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void RunLabel::setSelectionRange(int anchor, int pos) {
    selAnchor = anchor;
    selPos = pos;
    update();
}

QString RunLabel::selectedText() const {
    if (selAnchor < 0 || selPos < 0 || selAnchor == selPos) {
        return {};
    }
    return full.mid(qMin(selAnchor, selPos), qAbs(selPos - selAnchor));
}

QSize RunLabel::sizeHint() const { return QSize(0, heightForWidth(qMax(1, width()))); }

int RunLabel::heightForWidth(int width) const {
    qreal total = 0;
    int start = 0;
    while (true) {
        const int nl = static_cast<int>(full.indexOf(u'\n', start));
        QTextLayout probe;
        total += layoutInto(probe, nl < 0 ? full.mid(start) : full.mid(start, nl - start), font(),
                            width);
        if (nl < 0) {
            break;
        }
        start = nl + 1;
    }
    return qCeil(total) + 1;
}

void RunLabel::paintEvent(QPaintEvent *) {
    relayout(width());
    QPainter painter(this);
    const int paintWidth = qMax(1, width());
    for (size_t p = 0; p < layouts.size(); ++p) {
        // NOTE: draw() with an explicit formats vector drops every
        // range on this Qt version (probe-verified), and setFormats
        // clears laid-out lines — so install formats first, then
        // re-place lines, then draw.
        const QVector<QTextLayout::FormatRange> formats = formatsForParagraph(int(p));
        layouts[p]->setFormats(QList<QTextLayout::FormatRange>(formats.cbegin(), formats.cend()));
        placeLines(*layouts[p], paintWidth);
        layouts[p]->draw(&painter, QPointF(0, paraTops[int(p)]));
    }
}

QVector<QTextLayout::FormatRange> RunLabel::formatsForParagraph(int p) const {
    relayout(qMax(1, width()));
    if (p < 0 || p >= int(layouts.size())) {
        return {};
    }
    const int ps = paraStarts[p];
    const int pe = ps + static_cast<int>(layouts[size_t(p)]->text().size());
    QVector<QTextLayout::FormatRange> formats;
    int at = 0;
    for (const Run &run : runs) {
        const int end = at + static_cast<int>(run.text.size());
        const int from = qMax(at, ps);
        const int to = qMin(end, pe);
        if (to > from) {
            QTextCharFormat format;
            format.setForeground(inkColor(run.ink));
            format.setFont(runFont(run));
            // Hrefs read as links (blue through the Link role,
            // always underlined, like the old browser pane);
            // bare Link-ink runs (field tags) stay flat.
            if (!run.href.isEmpty()) {
                format.setFontUnderline(true);
            }
            formats << QTextLayout::FormatRange{from - ps, to - from, format};
        }
        at = end;
    }
    if (selAnchor >= 0 && selPos >= 0 && selAnchor != selPos) {
        const int from = qMax(qMin(selAnchor, selPos), ps);
        const int to = qMin(qMax(selAnchor, selPos), pe);
        if (to > from) {
            QTextCharFormat selected;
            selected.setBackground(palette().color(QPalette::Highlight));
            selected.setForeground(palette().color(QPalette::HighlightedText));
            formats << QTextLayout::FormatRange{from - ps, to - from, selected};
        }
    }
    return formats;
}

void RunLabel::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        setFocus(Qt::MouseFocusReason);
        hideTip();
        pressOffset = characterAt(event->pos());
        selAnchor = pressOffset;
        selPos = pressOffset;
        update();
        // Consumed: parent propagation would re-deliver the same
        // press to content/viewport filters (ArticleView's span
        // tracker must see it exactly once). App-level filters ran
        // first and are unaffected.
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void RunLabel::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::NoButton) {
        setCursor(linkAt(event->pos()).isEmpty() ? Qt::IBeamCursor : Qt::PointingHandCursor);
        updateTip(event->pos());
        return;
    }
    hideTip();
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

void RunLabel::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        const int offset = characterAt(event->pos());
        if (offset >= 0 && offset == pressOffset && selAnchor == selPos) {
            const QString href = linkAt(event->pos());
            if (!href.isEmpty()) {
                emit linkClicked(href);
            }
        }
        pressOffset = selectionNone;
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void RunLabel::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // Kanji dialog hook (the opener filters kana through kanji
        // info): surrogate-joined like the hover path.
        const int offset = characterAt(event->pos());
        const QString shown = text();
        if (offset >= 0 && offset < shown.size()) {
            emit kanjiDoubleClicked(shown.mid(offset, shown.at(offset).isHighSurrogate() ? 2 : 1));
        }
        event->accept();
    }
}

void RunLabel::leaveEvent(QEvent *event) {
    hideTip();
    QWidget::leaveEvent(event);
}

QString RunLabel::tipAt(const QPoint &pos) const {
    const int offset = characterAt(pos);
    if (offset < 0) {
        return {};
    }
    const int i = runIndexAt(offset);
    if (i < 0) {
        return {};
    }
    return runs[i].tip;
}

void RunLabel::updateTip(const QPoint &pos) {
    const QString tip = tipAt(pos);
    if (tip == tipText) {
        // Same description: running delay (or visible tip)
        // stands — re-arming per move would blink the tip out
        // while sweeping a tag (table precedent).
        return;
    }
    hideTip();
    if (tip.isEmpty()) {
        return;
    }
    tipText = tip;
    // Below the cursor: the tip must never cover the hovered run
    // (a tip under the cursor re-fires hover — the phantom-move
    // churn the kanji cards were built to avoid).
    tipAnchor = mapToGlobal(pos) + QPoint(12, 20);
    tipTimer->start();
}

void RunLabel::hideTip() {
    tipTimer->stop();
    tipText.clear();
    QToolTip::hideText();
}

void RunLabel::keyPressEvent(QKeyEvent *event) {
    if (event->matches(QKeySequence::Copy) && !selectedText().isEmpty()) {
        QApplication::clipboard()->setText(selectedText());
        return;
    }
    QWidget::keyPressEvent(event);
}

void RunLabel::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    laidWidth = -1;
    updateGeometry();
}

void RunLabel::changeEvent(QEvent *event) {
    if (event->type() == QEvent::FontChange) {
        laidWidth = -1;
        updateGeometry();
        update();
    }
    QWidget::changeEvent(event);
}
