#include "widgets/tooltip/KanjiHover.h"

#include "widgets/list/CandidateList.h"
#include "widgets/table/DictionaryTable.h"
#include "widgets/text/ArticleView.h"
#include "widgets/text/RubyLabel.h"
#include "widgets/text/RunLabel.h"

#include <QAbstractScrollArea>
#include <QFontMetrics>
#include <QListWidgetItem>
#include <QWidget>

#include "util/CellText.h"

void KanjiHover::remember(QWidget *target, const QPoint &local) {
    // Viewport-anchored: labels glide with the scroll, so the
    // re-hit point is stored in scroll-area viewport coords (same
    // viewport point = new content after a scroll).
    lastGlobal = target->mapToGlobal(local);
    lastArea.clear();
    for (QWidget *w = target; w != nullptr && w != window; w = w->parentWidget()) {
        if (auto *scroll = qobject_cast<QAbstractScrollArea *>(w)) {
            if (qobject_cast<DictionaryTable *>(w) == nullptr &&
                qobject_cast<ArticleView *>(w) == nullptr &&
                qobject_cast<CandidateList *>(w) == nullptr) {
                return;
            }
            lastArea = scroll;
            lastPoint =
                (target == scroll->viewport()) ? local : target->mapTo(scroll->viewport(), local);
            return;
        }
    }
}

bool KanjiHover::isHoverSurface(QWidget *target, const QPoint &local) const {
    for (QWidget *w = target; w != nullptr && w != window; w = w->parentWidget()) {
        if (auto *table = qobject_cast<DictionaryTable *>(w)) {
            return target == table->viewport();
        }
        if (auto *view = qobject_cast<ArticleView *>(w)) {
            if (target == view->viewport()) {
                // Gaps carry no text (matches the old per-widget
                // filter, which ignored viewport moves over no
                // label); labels resolve through kanjiAt.
                QPoint ignored;
                return view->labelAt(local, &ignored) != nullptr;
            }
            return qobject_cast<RunLabel *>(target) != nullptr ||
                   qobject_cast<RubyLabel *>(target) != nullptr;
        }
        if (auto *strip = qobject_cast<CandidateList *>(w)) {
            return target == strip->viewport();
        }
    }
    return false;
}

QString KanjiHover::kanjiAt(QObject *watched, const QPoint &local) const {
    QWidget *target = qobject_cast<QWidget *>(watched);
    if (target == nullptr) {
        return {};
    }
    for (QWidget *w = target; w != nullptr && w != window; w = w->parentWidget()) {
        if (auto *table = qobject_cast<DictionaryTable *>(w)) {
            if (target != table->viewport()) {
                return {}; // headers, scrollbars: no kanji there
            }
            return table->kanjiAt(local);
        }
        if (auto *view = qobject_cast<ArticleView *>(w)) {
            if (target == view->viewport()) {
                return view->kanjiAt(local);
            }
            auto *label = qobject_cast<QWidget *>(target);
            if (label != nullptr && (qobject_cast<RunLabel *>(label) != nullptr ||
                                     qobject_cast<RubyLabel *>(label) != nullptr)) {
                return view->kanjiAt(label->mapTo(view->viewport(), local));
            }
            return {};
        }
        if (auto *strip = qobject_cast<CandidateList *>(w)) {
            if (target != strip->viewport()) {
                return {};
            }
            const QListWidgetItem *item = strip->itemAt(local);
            if (item == nullptr) {
                return {};
            }
            // One card per glyph: resolve the character under the
            // cursor, not the whole candidate (multi-character ink
            // candidates otherwise never match a single-kanji
            // card). Cells center their text (see the candidate
            // delegate paint), so measure from the centered left.
            const QString text = item->text();
            const QRect cell = strip->visualItemRect(item);
            const QFontMetrics metrics(strip->font());
            const QString visible = metrics.elidedText(text, Qt::ElideRight, cell.width());
            const int textLeft =
                cell.left() + (cell.width() - metrics.horizontalAdvance(visible)) / 2;
            const int off = CellText::indexAtX(metrics, visible, local.x() - textLeft);
            if (off < 0 || off >= visible.size()) {
                return {};
            }
            const QChar c = visible.at(off);
            if (c.isHighSurrogate() && off + 1 < visible.size()) {
                return visible.mid(off, 2);
            }
            return QString(c);
        }
    }
    return {};
}
