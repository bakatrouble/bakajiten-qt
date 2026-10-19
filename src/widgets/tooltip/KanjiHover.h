#pragma once

#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QString>

class KanjiInfo;
class KanjiTooltip;
class QWidget;

/// Window-level kanji hover: one shared KanjiTooltip per top-level
/// window, driven by global mouse events. Installed as an
/// application event filter scoped to a single window; resolves the
/// widget under the event (tables, article labels, candidate
/// strips) and the kanji under the point, then shows/hides the one
/// shared card. Replaces the per-widget tip trackers (each table,
/// the article, and every candidate strip owned a card); tag tips,
/// hover wash, selection, scroll modes, and double-click paths are
/// untouched.
///
/// Moves carry their target widget (synthetic moves included), so
/// resolution never depends on the real cursor position; Leave and
/// scrolls re-resolve instead.
class KanjiHover : public QObject {
    Q_OBJECT
public:
    /// Window whose subtree is watched (the tip parents to it);
    /// kanjiInfo outlives this (owner's member, loaded in place).
    explicit KanjiHover(QWidget *window, KanjiInfo *kanjiInfo);

    /// Re-resolve under the last hover point (scrolls, selection
    /// clears that move no cursor).
    void rehit();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// Wire scrollbar/model/selection signals for a hovered scroll
    /// area (idempotent — late widgets and swapped selection
    /// models connect on first contact).
    void ensureWired(QWidget *area);
    /// Remember a hover point in scroll-area viewport coords, so
    /// scroll re-hits resolve the new content at the same point.
    /// Hit-testing lives in `KanjiHoverHit.cpp` (same split as the
    /// table's `DictionaryTableHover.cpp`).
    void remember(QWidget *target, const QPoint &local);
    /// Deepest-widget moves only (propagation hops to containers
    /// must not drive the card): table/strip viewports and article
    /// labels (viewport points need a label under them).
    bool isHoverSurface(QWidget *target, const QPoint &local) const;
    /// Kanji under a widget-local hover point, or empty (untracked
    /// widgets, kana, uncovered kanji all miss).
    QString kanjiAt(QObject *watched, const QPoint &local) const;
    /// Resolve `ch` to a card and hover/unhover (dedups repeats).
    void showFor(const QString &ch);

    QWidget *window = nullptr;
    KanjiInfo *kanjiInfo = nullptr;
    KanjiTooltip *tip = nullptr;
    QString lastChar;
    /// Last hover point: scroll area + viewport-local coords (plus
    /// the global fallback when the area is gone).
    QPointer<QWidget> lastArea;
    QPoint lastPoint;
    QPoint lastGlobal;
    /// Cursor inside a tracked widget (Leave/deactivate clears —
    /// scrolls with no cursor show nothing).
    bool cursorInside = false;
};
