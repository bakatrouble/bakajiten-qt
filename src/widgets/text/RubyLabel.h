#pragma once

#include <QPoint>
#include <QRect>
#include <QString>
#include <QVector>
#include <QWidget>

/// Headword with furigana (single line): ruby readings painted
/// small above their kanji spans, kana runs plain — all at heading
/// size. Hit-testing maps a position back to a base character for
/// kanji hover cards. Replaces the ruby-table HTML of the old
/// article (Qt has no native ruby).
class RubyLabel : public QWidget {
    Q_OBJECT
public:
    /// One column: kana run (empty ruby) or kanji base + reading.
    /// Strings arrive display-ready (Romaji setting applied
    /// upstream: kana bases may read as romaji); `keb` carries the
    /// raw written-form segment so selection copies kana, never
    /// romaji (empty = base, for hand-built spans).
    struct Span {
        QString base;
        QString ruby;
        QString keb;
    };

    explicit RubyLabel(QWidget *parent = nullptr);

    void setSpans(const QVector<Span> &spans);
    /// Concatenated base text.
    QString text() const;
    /// Base-text offset under pos, or -1 outside text.
    int characterAt(const QPoint &pos) const;
    /// Base-text rect for one offset (for tests).
    QRect characterRect(int offset) const;
    /// Selected written-form text (keb, never display romaji).
    QString selectedText() const;

    QSize sizeHint() const override;

signals:
    /// Double-clicked character (raw, may be kana — the opener
    /// filters through kanji info).
    void kanjiDoubleClicked(const QString &ch);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    QFont baseFont() const;
    QFont rubyFont() const;
    /// Column widths (max of base/ruby advances per span).
    QVector<int> columnWidths() const;
    QVector<int> columnsFor(const QVector<Span> &list) const;
    /// Advance of one user-perceived character (surrogate pairs
    /// move as one, matching the tooltip half of the hover path).
    static int charAdvance(const QFontMetrics &metrics, const QString &text, int &i);
    /// Spans fitted to the current width (see .cpp).
    QVector<Span> shownSpans() const;
    /// Shown base-text length (offsets live in this space).
    int shownSize() const;
    int spanIndexAt(int x) const;

    QVector<Span> spans;
    QString full;
    int selAnchor = -1;
    int selPos = -1;
};
