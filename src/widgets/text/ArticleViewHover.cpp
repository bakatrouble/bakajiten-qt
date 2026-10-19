#include "widgets/text/ArticleView.h"

#include "widgets/text/RubyLabel.h"
#include "widgets/text/RunLabel.h"

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QStringList>

QString ArticleView::kanjiAt(const QPoint &viewportPos) const {
    QPoint local;
    QWidget *label = labelAt(viewportPos, &local);
    int offset = -1;
    if (auto *ruby = qobject_cast<RubyLabel *>(label)) {
        offset = ruby->characterAt(local);
        const QString text = ruby->text();
        if (offset >= 0 && offset < text.size()) {
            return text.mid(offset, text.at(offset).isHighSurrogate() ? 2 : 1);
        }
        return {};
    }
    if (auto *runs = qobject_cast<RunLabel *>(label)) {
        offset = runs->characterAt(local);
        if (offset >= 0) {
            const QString text = runs->text();
            if (offset < text.size()) {
                return text.mid(offset, text.at(offset).isHighSurrogate() ? 2 : 1);
            }
        }
    }
    return {};
}

QString ArticleView::linkAt(const QPoint &viewportPos) const {
    QPoint local;
    if (auto *runs = qobject_cast<RunLabel *>(labelAt(viewportPos, &local))) {
        return runs->linkAt(local);
    }
    return {};
}

QList<ArticleView::LinkHit> ArticleView::linkHits() const {
    QList<LinkHit> out;
    if (content == nullptr) {
        return out;
    }
    const QList<RunLabel *> labels =
        content->findChildren<RunLabel *>(QString(), Qt::FindDirectChildrenOnly);
    for (RunLabel *label : labels) {
        for (const auto &[rect, href] : label->links()) {
            const QPoint at = label->mapTo(viewport(), rect.topLeft());
            out << LinkHit{QRect(at, rect.size()), href, label};
        }
    }
    return out;
}

bool ArticleView::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        // New gesture resets the span; the pressed label maintains
        // its own single-label state afterwards.
        const auto *me = static_cast<const QMouseEvent *>(event);
        hasSpan = false;
        spanning = false;
        spanAnchor.clear();
        spanAnchorOff = -1;
        if (me->button() == Qt::LeftButton) {
            if (auto *runs = qobject_cast<RunLabel *>(watched);
                runs != nullptr && runs->objectName() == QStringLiteral("articleSense")) {
                spanAnchor = runs;
                spanAnchorOff = runs->characterAt(me->pos());
            }
            for (RunLabel *sense : senseLabels()) {
                if (sense != spanAnchor.data()) {
                    setRange(sense, -1, -1);
                }
            }
        }
        return false;
    }
    if (event->type() == QEvent::MouseButtonRelease) {
        if (static_cast<const QMouseEvent *>(event)->button() == Qt::LeftButton) {
            // Persistent only across labels, idempotent across
            // propagation echoes (releases fall through to parents).
            // A new press clears it. The home label keeps whatever
            // its own handlers left (links included).
            if (spanning) {
                hasSpan = true;
            }
            spanning = false;
        }
        return false;
    }
    if (event->type() == QEvent::KeyPress) {
        const auto *ke = static_cast<const QKeyEvent *>(event);
        if (ke->matches(QKeySequence::Copy) && hasSpan) {
            // Runs before the focused label's own copy (filters see
            // events first): one clipboard write for the whole span.
            QApplication::clipboard()->setText(selectedText());
            return true;
        }
        return false;
    }
    if (event->type() == QEvent::MouseMove) {
        // Press-drag extends the span; hover cards resolve
        // window-wide (see KanjiHover).
        const auto *me = static_cast<const QMouseEvent *>(event);
        if ((me->buttons() & Qt::LeftButton) != 0 && spanAnchor != nullptr) {
            updateSpan(watched, me->pos());
        }
        return false;
    }
    return QScrollArea::eventFilter(watched, event);
}

QWidget *ArticleView::labelAt(const QPoint &viewportPos, QPoint *local) const {
    QWidget *page = content;
    if (page == nullptr) {
        return nullptr;
    }
    QWidget *top = viewport()->childAt(viewportPos);
    if (top == nullptr || top == page) {
        // Gaps hit the content itself (or nothing): only labels
        // carry hoverable text.
        if (top == page && local != nullptr) {
            QWidget *label = page->childAt(page->mapFrom(viewport(), viewportPos));
            if (qobject_cast<RunLabel *>(label) != nullptr ||
                qobject_cast<RubyLabel *>(label) != nullptr) {
                *local = label->mapFrom(viewport(), viewportPos);
                return label;
            }
        }
        return nullptr;
    }
    if (qobject_cast<RunLabel *>(top) != nullptr || qobject_cast<RubyLabel *>(top) != nullptr) {
        if (local != nullptr) {
            *local = top->mapFrom(viewport(), viewportPos);
        }
        return top;
    }
    return nullptr;
}

QList<RunLabel *> ArticleView::senseLabels() const {
    QList<RunLabel *> out;
    if (content == nullptr) {
        return out;
    }
    for (RunLabel *label :
         content->findChildren<RunLabel *>(QString(), Qt::FindDirectChildrenOnly)) {
        if (label->objectName() == QStringLiteral("articleSense")) {
            out << label;
        }
    }
    return out;
}

void ArticleView::setRange(RunLabel *label, int anchor, int pos) {
    if (label != nullptr) {
        label->setSelectionRange(anchor, pos);
    }
}

QString ArticleView::selectedText() const {
    QStringList blocks;
    for (RunLabel *sense : senseLabels()) {
        const QString part = sense->selectedText();
        if (!part.isEmpty()) {
            blocks << part;
        }
    }
    return blocks.join(QStringLiteral("\n"));
}

static int textSize(RunLabel *label) { return static_cast<int>(label->text().size()); }

void ArticleView::updateSpan(QObject *watched, const QPoint &at) {
    auto *anchor = qobject_cast<RunLabel *>(spanAnchor.data());
    const QList<RunLabel *> senses = senseLabels();
    const int ia = static_cast<int>(senses.indexOf(anchor));
    if (anchor == nullptr || ia < 0 || spanAnchorOff < 0) {
        spanning = false;
        return;
    }
    // Hit-test in viewport coords directly: watched-local points
    // map exactly (the press widget grabs out-of-bounds drags
    // too), with no roundtrip through global coordinates — which
    // offscreen platforms report with an offset.
    auto *watchWidget = qobject_cast<QWidget *>(watched);
    if (watchWidget == nullptr) {
        return;
    }
    const QPoint viewportAt = (watchWidget == viewport()) ? at : watchWidget->mapTo(viewport(), at);
    QPoint local;
    RunLabel *focus = nullptr;
    int focusOff = -1;
    if (QWidget *under = labelAt(viewportAt, &local)) {
        focus = qobject_cast<RunLabel *>(under);
        if (focus != nullptr) {
            focusOff = focus->characterAt(local);
        }
    }
    if (focus != nullptr && focus->objectName() == QStringLiteral("articleSense") &&
        senses.contains(focus) && focus != anchor) {
        // Past-the-text points (line gaps, padding) clamp to the
        // line under the cursor — never the whole label.
        int edge = focusOff;
        if (edge < 0) {
            edge = focus->nearestOffset(local);
        }
        const int ib = static_cast<int>(senses.indexOf(focus));
        const int anchorSize = textSize(anchor);
        const int focusSize = textSize(focus);
        if (ia < ib) {
            setRange(anchor, spanAnchorOff, anchorSize);
            for (int i = ia + 1; i < ib; ++i) {
                setRange(senses[i], 0, textSize(senses[i]));
            }
            setRange(focus, 0, edge);
        } else {
            setRange(anchor, 0, spanAnchorOff);
            for (int i = ib + 1; i < ia; ++i) {
                setRange(senses[i], 0, textSize(senses[i]));
            }
            setRange(focus, edge, focusSize);
        }
        // Beyond the focus stays clear: gap transit below may have
        // filled it (the span is exactly the anchor–focus range).
        for (int i = 0; i < qMin(ia, ib); ++i) {
            setRange(senses[i], -1, -1);
        }
        for (int i = qMax(ia, ib) + 1; i < senses.size(); ++i) {
            setRange(senses[i], -1, -1);
        }
        spanning = true;
        return;
    }
    if (focus == anchor) {
        // Back home: the label maintains itself, others clear.
        spanning = false;
        for (RunLabel *sense : senses) {
            if (sense != anchor) {
                setRange(sense, -1, -1);
            }
        }
        return;
    }
    // Unqualified focus (heading, gaps): extend through the senses
    // the cursor has passed in the drag direction; anything beyond
    // stays clear (a straight-down drag through the gaps must not
    // pre-select every meaning). Overshoot past the last sense
    // still keeps the full span — every sense top/bottom has
    // passed by then.
    const int anchorTop = anchor->mapTo(viewport(), QPoint(0, 0)).y();
    if (viewportAt.y() >= anchorTop) {
        setRange(anchor, spanAnchorOff, textSize(anchor));
        for (int i = ia + 1; i < senses.size(); ++i) {
            // Passed (top reached) reads full; unreached clears.
            const int top = senses[i]->mapTo(viewport(), QPoint(0, 0)).y();
            if (top <= viewportAt.y()) {
                setRange(senses[i], 0, textSize(senses[i]));
            } else {
                setRange(senses[i], -1, -1);
            }
        }
        for (int i = 0; i < ia; ++i) {
            setRange(senses[i], -1, -1);
        }
    } else {
        setRange(anchor, 0, spanAnchorOff);
        for (int i = 0; i < ia; ++i) {
            // Passed (bottom reached) reads full; unreached clears.
            const int bottom = senses[i]->mapTo(viewport(), QPoint(0, senses[i]->height())).y();
            if (bottom >= viewportAt.y()) {
                setRange(senses[i], 0, textSize(senses[i]));
            } else {
                setRange(senses[i], -1, -1);
            }
        }
        for (int i = ia + 1; i < senses.size(); ++i) {
            setRange(senses[i], -1, -1);
        }
    }
    spanning = true;
}
