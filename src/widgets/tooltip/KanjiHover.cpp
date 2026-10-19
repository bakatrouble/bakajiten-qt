#include "widgets/tooltip/KanjiHover.h"

#include "core/dictionary/KanjiInfo.h"
#include "widgets/list/CandidateList.h"
#include "widgets/table/DictionaryTable.h"
#include "widgets/text/ArticleView.h"
#include "widgets/tooltip/KanjiTooltip.h"

#include <QAbstractItemModel>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QEvent>
#include <QItemSelectionModel>
#include <QMouseEvent>
#include <QObject>
#include <QScrollBar>
#include <QString>
#include <QWidget>
#include <Qt>

KanjiHover::KanjiHover(QWidget *window, KanjiInfo *kanjiInfo)
    : QObject(window), window(window), kanjiInfo(kanjiInfo) {
    tip = new KanjiTooltip(window);
    tip->setObjectName(QStringLiteral("kanjiTip"));
    qApp->installEventFilter(this);
    // Pre-wire what exists now (thumb drags and model resets
    // re-resolve without a prior move); late widgets wire lazily
    // on their first move (see ensureWired).
    for (auto *table : window->findChildren<DictionaryTable *>()) {
        ensureWired(table);
    }
    for (auto *view : window->findChildren<ArticleView *>()) {
        ensureWired(view);
    }
    for (auto *strip : window->findChildren<CandidateList *>()) {
        ensureWired(strip);
    }
}

void KanjiHover::ensureWired(QWidget *area) {
    // Content moves under a stationary cursor (wheel glides,
    // thumb drags, animated strips) and swaps on selection/model
    // resets: re-resolve instead of going stale. Unique connects
    // make repeat wiring (and selection-model swaps) safe.
    auto *scroll = qobject_cast<QAbstractScrollArea *>(area);
    if (scroll != nullptr) {
        connect(scroll->verticalScrollBar(), &QScrollBar::valueChanged, this, &KanjiHover::rehit,
                Qt::UniqueConnection);
        connect(scroll->horizontalScrollBar(), &QScrollBar::valueChanged, this, &KanjiHover::rehit,
                Qt::UniqueConnection);
    }
    if (auto *table = qobject_cast<DictionaryTable *>(area)) {
        // Selection swaps content under a static cursor (clicks
        // re-lay-out the splitter, Escape clears). The press/drag
        // guard inside rehit keeps clicks from flashing.
        if (table->selectionModel() != nullptr) {
            connect(table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                    &KanjiHover::rehit, Qt::UniqueConnection);
        }
    }
    if (auto *strip = qobject_cast<CandidateList *>(area)) {
        // setHits clears rows under a static cursor.
        connect(strip->model(), &QAbstractItemModel::rowsRemoved, this, &KanjiHover::rehit,
                Qt::UniqueConnection);
        connect(strip->model(), &QAbstractItemModel::modelReset, this, &KanjiHover::rehit,
                Qt::UniqueConnection);
    }
}

bool KanjiHover::eventFilter(QObject *watched, QEvent *event) {
    auto *target = qobject_cast<QWidget *>(watched);
    if (target == nullptr || target->window() != window) {
        return false;
    }
    const QEvent::Type type = event->type();
    if (type == QEvent::MouseMove) {
        const auto *me = static_cast<const QMouseEvent *>(event);
        // Propagation: an unaccepted move bubbles from the deepest
        // widget to its parents, and the application filter observes
        // every hop. Only the deepest tracked surface drives the
        // card; container hops (content, headers, inputs) are
        // ignored so they can't cancel a pending show.
        if (!isHoverSurface(target, me->pos())) {
            return false;
        }
        // Late widgets (and swapped selection models) wire on
        // first contact.
        for (QWidget *w = target; w != nullptr && w != window; w = w->parentWidget()) {
            if (qobject_cast<DictionaryTable *>(w) != nullptr ||
                qobject_cast<ArticleView *>(w) != nullptr ||
                qobject_cast<CandidateList *>(w) != nullptr) {
                ensureWired(w);
                break;
            }
        }
        remember(target, me->pos());
        if (me->buttons() != Qt::NoButton) {
            return false; // press-drag selects text; no hover cards
        }
        cursorInside = true;
        showFor(kanjiAt(watched, me->pos()));
        return false;
    }
    if (type == QEvent::MouseButtonPress) {
        // Press hides (drag-select must not trail a card;
        // double-click paths are separate signals). Cleared, so
        // the next move re-shows; the rehit button-guard keeps
        // the click's own selection change from flashing.
        lastChar.clear();
        tip->unhover();
        return false;
    }
    if (type == QEvent::Wheel) {
        // Content scrolled under a static cursor: strips hide at
        // once (cells glide away), everything else re-resolves.
        QWidget *host = target;
        while (host != nullptr && host != window) {
            if (qobject_cast<CandidateList *>(host) != nullptr) {
                lastChar.clear();
                tip->hide();
                tip->unhover();
                return false;
            }
            if (qobject_cast<DictionaryTable *>(host) != nullptr ||
                qobject_cast<ArticleView *>(host) != nullptr) {
                break;
            }
            host = host->parentWidget();
        }
        rehit();
        return false;
    }
    if (type == QEvent::Leave) {
        // Grace dismisses; reaching the card cancels it, a genuine
        // departure hides after the delay. Cursor flagged out so a
        // scroll without the cursor shows no phantom card.
        cursorInside = false;
        lastChar.clear();
        tip->unhover();
        return false;
    }
    if (type == QEvent::WindowDeactivate && watched == window) {
        cursorInside = false;
        tip->hide();
        lastChar.clear();
        return false;
    }
    return false;
}

void KanjiHover::rehit() {
    if (!cursorInside || QApplication::mouseButtons() != Qt::NoButton) {
        // Cursor gone (Leave/deactivate) or mid-gesture
        // (press-drag selects): never a phantom card.
        showFor(QString());
        return;
    }
    if (!lastArea.isNull()) {
        // Strips glide/refresh without cursor motion (wheel
        // animation, setHits): never re-show without a move, just
        // drop a stale card.
        if (qobject_cast<CandidateList *>(lastArea) != nullptr) {
            showFor(QString());
            return;
        }
        if (auto *scroll = qobject_cast<QAbstractScrollArea *>(lastArea)) {
            showFor(kanjiAt(scroll->viewport(), lastPoint));
            return;
        }
    }
    QWidget *under = QApplication::widgetAt(lastGlobal);
    if (under == nullptr || under->window() != window) {
        showFor(QString());
        return;
    }
    showFor(kanjiAt(under, under->mapFromGlobal(lastGlobal)));
}

void KanjiHover::showFor(const QString &ch) {
    if (ch == lastChar) {
        return;
    }
    lastChar = ch;
    if (ch.isEmpty()) {
        tip->unhover();
        return;
    }
    const KanjiInfo::Display card =
        kanjiInfo != nullptr ? kanjiInfo->display(ch) : KanjiInfo::Display{};
    if (!card.valid) {
        // Kana (or uncovered kanji): no stale card from the last
        // hovered glyph.
        lastChar.clear();
        tip->unhover();
        return;
    }
    tip->hover(ch, card);
}
