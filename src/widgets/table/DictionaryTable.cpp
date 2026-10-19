#include "widgets/table/DictionaryTable.h"

#include "core/settings/MemorizedState.h"
#include "widgets/tooltip/Tooltip.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QObject>
#include <QScrollBar>
#include <QTableView>
#include <QTimer>
#include <QToolTip>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>

DictionaryTable::DictionaryTable(QWidget *parent) : QTableView(parent) {
    // Single-line elided cells (see the delegate): one fixed height
    // for every row, derived from the largest cell font so no row
    // ever measures. Refit on font changes (a scaled interface font
    // in fixed-height rows clips).
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    fitRowHeight();
    setWordWrap(false);
    // Meaning column deliberately oversized (never stretched):
    // long entries always keep horizontal scroll available.
    setColumnWidth(2, 6000);
    // Row numbers carry no meaning on search results: one header
    // (the column titles) is enough chrome.
    verticalHeader()->hide();
    horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    tagTimer = new QTimer(this);
    tagTimer->setSingleShot(true);
    tagTimer->setInterval(Tooltip::showDelayMs);
    connect(tagTimer, &QTimer::timeout, this, [this] {
        if (!tagTip.isEmpty()) {
            QToolTip::showText(tagAnchor, tagTip, viewport());
        }
    });
    connect(horizontalHeader(), &QHeaderView::sectionResized, this,
            &DictionaryTable::onColumnResized);
    // Scroll moves rows under a stationary cursor: re-run hover
    // at the last mouse position.
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &DictionaryTable::onScrolled);
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, &DictionaryTable::onScrolled);
    // Whole-row selection on click (single row at a time): clicks
    // highlight the full row, the hover wash owns unselected
    // feedback, text selection owns substring feedback. No rubber
    // band (single selection), so press-drag stays a text gesture.
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    // Per-pixel scrolling (rows move smoothly under a stationary
    // cursor).
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    viewport()->setMouseTracking(true);
    viewport()->installEventFilter(this);

    setFrameShape(StyledPanel);
    setFrameShadow(Sunken);
}

void DictionaryTable::wheelEvent(QWheelEvent *event) {
    // Scrolling under a static cursor must not leave a stale tag
    // tip; the scroll itself stays default (per-pixel detents, no
    // glide). Kanji cards re-resolve window-wide (see KanjiHover).
    hideTagTip();
    QTableView::wheelEvent(event);
}

void DictionaryTable::scrollTo(const QModelIndex &index, ScrollHint hint) {
    // Row selection parks current in the last (meaning) column —
    // Qt's selectRow picks it — and EnsureVisible would yank the
    // horizontal scrollbar there on every row change (including
    // article open). Current itself is pinned to the heading column
    // (see pinCurrentToText); this drops the scroll half.
    if (index.isValid() && index.column() == 2 && hint == QAbstractItemView::EnsureVisible) {
        return;
    }
    QTableView::scrollTo(index, hint);
}

bool DictionaryTable::eventFilter(QObject *watched, QEvent *event) {
    if (watched == viewport()) {
        if (event->type() == QEvent::MouseMove) {
            const auto *me = static_cast<const QMouseEvent *>(event);
            lastMouse = me->pos();
            mouseInside = true;
            if (me->buttons() == Qt::NoButton) {
                setHoverRow(rowAt(me->pos().y()));
            } else if (selecting && (me->buttons() & Qt::LeftButton) != 0) {
                moveAt(me->pos());
            }
            updateCursor(me->pos());
            updateTagTip(me->pos());
        } else if (event->type() == QEvent::Leave) {
            // Overlay tolerance: our own tooltip window popping up
            // over a stationary cursor fires a Leave although
            // nothing moved (the card parks at cursor + 16px, and
            // clamping can push it back over the cursor). The
            // cursor still sitting at the last hover point proves
            // it — a genuine departure always moves it elsewhere
            // first.
            if ((viewport()->mapFromGlobal(QCursor::pos()) - lastMouse).manhattanLength() <= 2) {
                return QTableView::eventFilter(watched, event);
            }
            mouseInside = false;
            setHoverRow(-1);
            viewport()->unsetCursor();
            hideTagTip();
        } else if (event->type() == QEvent::MouseButtonPress) {
            const auto *me = static_cast<const QMouseEvent *>(event);
            hideTagTip();
            if (me->button() == Qt::LeftButton) {
                pressAt(me->pos());
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            const auto *me = static_cast<const QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                selecting = false;
            }
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            const auto *me = static_cast<const QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                // Kanji dialog hook (the opener filters kana through
                // kanji info); drag selection keeps working — the
                // presses already ran through pressAt/moveAt.
                const QString ch = kanjiAt(me->pos());
                if (!ch.isEmpty()) {
                    emit kanjiDoubleClicked(ch);
                }
            }
        }
    }
    return QTableView::eventFilter(watched, event);
}

void DictionaryTable::keyPressEvent(QKeyEvent *event) {
    if (event->matches(QKeySequence::Copy) && hasTextSelection()) {
        QApplication::clipboard()->setText(selectedText());
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape &&
        (hasTextSelection() || (selectionModel() != nullptr && selectionModel()->hasSelection()))) {
        clearSelections();
        event->accept();
        return;
    }
    QTableView::keyPressEvent(event);
}

void DictionaryTable::fitRowHeight() {
    QFont heading = font();
    heading.setBold(true);
    heading.setPointSizeF(heading.pointSizeF() + 2.0);
    verticalHeader()->setDefaultSectionSize(QFontMetrics(heading).lineSpacing() + 12);
}

void DictionaryTable::changeEvent(QEvent *event) {
    if (event->type() == QEvent::FontChange) {
        fitRowHeight();
    }
    QTableView::changeEvent(event);
}

void DictionaryTable::setModel(QAbstractItemModel *model) {
    QTableView::setModel(model);
    // Persisted heading/reading widths (the meaning column
    // stretches): applied here, not the ctor — setting the model
    // resets header section sizes to defaults.
    const int headingWidth = MemorizedState::resultsHeadingWidth();
    const int readingWidth = MemorizedState::resultsReadingWidth();
    if (headingWidth > 0) {
        setColumnWidth(0, headingWidth);
    }
    if (readingWidth > 0) {
        setColumnWidth(1, readingWidth);
    }
    // Meaning column deliberately oversized (never stretched):
    // long entries always keep horizontal scroll available.
    // Re-applied here — the reset above wipes the ctor value.
    setColumnWidth(2, 6000);
    // Rows change identity on every search: stale offsets would
    // paint on the wrong row, and a stale row must not keep its
    // article.
    connect(model, &QAbstractItemModel::modelAboutToBeReset, this,
            &DictionaryTable::clearSelections, Qt::UniqueConnection);
    // A new result set starts at the top (same hook: the display
    // refresh path emits dataChanged, not a reset, so a reading
    // toggle keeps its scroll).
    connect(model, &QAbstractItemModel::modelAboutToBeReset, this, &DictionaryTable::resetScroll,
            Qt::UniqueConnection);
    // Row selection lands current in the meaning column (see
    // pinCurrentToText): keep it on the heading column instead.
    // Connected here, not the ctor — the selection model only
    // exists once a model is set.
    connect(selectionModel(), &QItemSelectionModel::currentChanged, this,
            &DictionaryTable::pinCurrentToText, Qt::UniqueConnection);
    connect(model, &QAbstractItemModel::modelAboutToBeReset, this, &DictionaryTable::hideTagTip,
            Qt::UniqueConnection);
}

void DictionaryTable::setHoverRow(int row) {
    if (row == hover) {
        return;
    }
    hover = row;
    viewport()->update();
}
