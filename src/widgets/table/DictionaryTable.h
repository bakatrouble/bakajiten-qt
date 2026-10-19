#pragma once

#include <QTableView>

#include <QString>

class QScrollBar;
class QKeyEvent;
class QWheelEvent;
class QModelIndex;
class KanjiInfo;
class KanjiTooltip;

/// Results table: uniform single-line rows (any row count scrolls
/// with zero measuring) with default wheel behavior. Scroll modes
/// stay per-pixel.
///
/// Heading/reading cells carry direct text selection (no editors):
/// I-beam on hover, press-drag selects a substring, Ctrl+C copies,
/// Escape or a new search clears. Clicks select the whole row
/// (single selection); the delegate paints the text selection.
class DictionaryTable : public QTableView {
    Q_OBJECT
public:
    explicit DictionaryTable(QWidget *parent = nullptr);

signals:
    /// Double-clicked character anywhere in a cell (raw — the
    /// opener filters kana through kanji info).
    void kanjiDoubleClicked(const QString &ch);

public:
    /// Hovered row (-1 = none): the delegate washes it (highlight,
    /// not selection — clicks/keyboard own selection separately).
    int hoverRow() const { return hover; }

    /// Active text selection (valid only when `hasTextSelection()`):
    /// UTF-16 offsets into the cell's full string.
    struct TextSelection {
        int row = -1;
        int column = -1;
        int start = 0;
        int length = 0;
    };
    bool hasTextSelection() const { return selection.length > 0; }
    TextSelection textSelection() const { return selection; }
    /// Selected substring (empty without a selection).
    QString selectedText() const;
    /// Drop text and row selection (rows change identity on reset;
    /// Escape; a cleared row must not keep its article).
    void clearSelections();
    /// Current index changed: pin it to the heading column (see
    /// Select.cpp).
    void pinCurrentToText(const QModelIndex &current);
    /// Never auto-scroll the 6000px meaning column into view (row
    /// selection parks current there; see pinCurrentToText).
    /// Keyboard navigation runs on column 0 and passes through.
    void scrollTo(const QModelIndex &index, ScrollHint hint) override;
    /// Kanji under the cursor, if any (columns 0–1 by shared
    /// geometry, column 2 by run walk). Hit-testing for the
    /// window-level hover; display stays with the shared card.
    QString kanjiAt(const QPoint &pos) const;
    /// Reinstall hook: clears the selection when rows reset.
    void setModel(QAbstractItemModel *model) override;

protected:
    void wheelEvent(QWheelEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    /// Interface font changed: rows keep the heading-derived height.
    void changeEvent(QEvent *event) override;

private:
    /// One fixed height for every row, from the largest cell font.
    void fitRowHeight();
    /// Track the hovered row, repainting on change.
    void setHoverRow(int row);
    /// Begin/extend the drag selection from a viewport position.
    void pressAt(const QPoint &pos);
    void moveAt(const QPoint &pos);
    /// UTF-16 offset into one cell's full string at viewport x.
    int charAt(int row, int column, int x) const;
    /// One character at a UTF-16 offset (surrogate-aware).
    static QString charAtVisible(const QString &text, int off);
    /// I-beam over heading/reading cells, arrow anywhere else.
    void updateCursor(const QPoint &pos);
    /// Tag description under the cursor, if any (meaning-column
    /// tag runs carry their tooltips).
    QString tagTipAt(const QPoint &pos) const;
    /// Arm, show, or dismiss the native tag tooltip.
    void updateTagTip(const QPoint &pos);
    void hideTagTip();
    /// New result set: jump back to the top.
    void resetScroll();
    /// Memorize a resized heading/reading width.
    void onColumnResized(int logical, int oldSize, int newSize);
    /// Content scrolled under a stationary cursor: re-run the hover
    /// state (row, cursor, cards) at the last mouse position.
    void onScrolled();

    int hover = -1;
    TextSelection selection;
    bool selecting = false;
    int anchor = 0;
    /// Pending native tag tooltip (delayed show, so sweeps don't
    /// flicker; placed below the cursor so it never covers the
    /// hovered run).
    QTimer *tagTimer = nullptr;
    QString tagTip;
    QPoint tagAnchor;
    /// Last mouse position (viewport coords) + inside flag: scroll
    /// fires no MouseMove, so the scroll hook re-runs hover here.
    QPoint lastMouse;
    bool mouseInside = false;
};
