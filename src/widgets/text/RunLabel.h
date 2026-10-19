#pragma once

#include <QPair>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QTextLayout>
#include <QVector>
#include <QWidget>

#include <cstdint>
#include <memory>
#include <vector>

class QTimer;

/// One wrapped article line block (a sense, a reading, notes): colored
/// runs with links, laid out by QTextLayout. Paint, hover, and
/// selection share the layout, so hit-testing always matches drawn
/// glyphs. Replaces one rich-text paragraph of the old QTextBrowser
/// article (no HTML anywhere in the pane).
class RunLabel : public QWidget {
    Q_OBJECT
public:
    /// Palette-resolved ink (never baked, so theme switches apply
    /// without rebuilding).
    enum class Ink : std::uint8_t { Body, Dim, Green, Red, Link };
    struct Run {
        QString text;
        Ink ink = Ink::Body;
        bool small = false;
        bool bold = false;
        /// Meta-reference query (empty = plain text).
        QString href;
        /// Tag description for hover tooltips (empty = no tip).
        QString tip;
    };

    explicit RunLabel(QWidget *parent = nullptr);

    void setRuns(const QVector<Run> &runs);
    /// Concatenated run text (paragraph breaks preserved).
    QString text() const;
    /// UTF-16 offset into text() under pos, or -1 outside text.
    int characterAt(const QPoint &pos) const;
    /// Nearest valid offset for any point: x outside the line
    /// clamps to that line's edge, y outside all lines to the
    /// nearest end. Span drags use this (past-the-text must not
    /// read as the whole label); single-label gestures keep the
    /// strict -1 contract of `characterAt`.
    int nearestOffset(const QPoint &pos) const;
    /// Href under pos, or empty.
    QString linkAt(const QPoint &pos) const;
    /// Programmatic selection for cross-label spans (ArticleView):
    /// anchor/pos in text() offsets, (-1, -1) clears.
    void setSelectionRange(int anchor, int pos);
    /// Format ranges for paragraph p (paragraph-relative offsets):
    /// paint draws them, tests inspect link styling. Lays out first.
    QVector<QTextLayout::FormatRange> formatsForParagraph(int p) const;
    /// Link rects in widget coords (multi-line links split).
    QList<QPair<QRect, QString>> links() const;
    QString selectedText() const;

    QSize sizeHint() const override;
    int heightForWidth(int width) const override;

signals:
    void linkClicked(const QString &href);
    /// Double-clicked character (raw, may be kana — the opener
    /// filters through kanji info).
    void kanjiDoubleClicked(const QString &ch);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    QFont runFont(const Run &run) const;
    QColor inkColor(Ink ink) const;
    void relayout(int width) const;
    void relayout() { relayout(width()); }
    static qreal layoutInto(QTextLayout &layout, const QString &text, const QFont &font, int width);
    /// (Re)creates laid-out lines. setFormats() clears lines, so
    /// paint re-places after installing formats (probe-verified);
    /// formats survive clearLayout.
    static qreal placeLines(QTextLayout &layout, int width);
    /// Paragraph index containing y (widget coords), or -1 in gaps.
    int paragraphAt(qreal y) const;
    int runIndexAt(int offset) const;
    /// Tag description under pos, or empty (mirrors the table's
    /// tagTipAt over run offsets instead of cell pixels).
    QString tipAt(const QPoint &pos) const;
    /// Arm (shared show delay) or stand (same text) the native tag
    /// tip.
    void updateTip(const QPoint &pos);
    void hideTip();

    QVector<Run> runs;
    QString full;
    /// One layout per \n-separated paragraph: QTextLayout ignores
    /// embedded newlines, so a single layout glues "See also" onto
    /// the gloss. Starts/tops/heights align with layouts by index;
    /// all offsets stay global into full (paragraph p covers
    /// [paraStarts[p], paraStarts[p] + layouts[p]->text().size())).
    mutable std::vector<std::unique_ptr<QTextLayout>> layouts;
    mutable QVector<int> paraStarts;
    mutable QVector<qreal> paraTops;
    mutable QVector<qreal> paraHeights;
    mutable int laidWidth = -1;
    int selAnchor = -1;
    int selPos = -1;
    int pressOffset = -1;
    QTimer *tipTimer = nullptr;
    QString tipText;
    QPoint tipAnchor;
};
