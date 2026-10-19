#pragma once

#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QScrollArea>
#include <QString>

struct ArticleDisplay;

class RunLabel;

/// Dictionary article as widgets (no rich text): heading ruby,
/// reading, badges, one run block per sense, other forms — laid out
/// in a scrolling column. Hover hit-testing and links come from the
/// labels; this view tracks the cursor for scroll re-hit-testing
/// and re-emits outward.
class ArticleView : public QScrollArea {
    Q_OBJECT
public:
    explicit ArticleView(QWidget *parent = nullptr);

    void setArticle(const ArticleDisplay &article);
    /// Kanji character under a viewport position (surrogate-joined),
    /// or empty.
    QString kanjiAt(const QPoint &viewportPos) const;
    /// Meta-link query under a viewport position, or empty.
    QString linkAt(const QPoint &viewportPos)
        const; /// All link hits: viewport-coord rect, query, target label.
    struct LinkHit {
        QRect rect;
        QString href;
        QWidget *label = nullptr;
    };
    QList<LinkHit> linkHits() const;
    /// Label under a viewport position for test event targeting
    /// (real moves land on labels directly; synthetic viewport
    /// moves do not reach filters). Null when over gaps.
    QWidget *labelAt(const QPoint &viewportPos, QPoint *local = nullptr) const;
    /// Cross-meaning selection text (sense blocks only — headings
    /// keep their own single-label selection), blocks joined by
    /// newlines. Empty without a span.
    QString selectedText() const;
    /// Re-apply badge stylesheets after a theme switch (paints read
    /// the live palette; stylesheets bake).
    void refreshTheme();

signals:
    void linkActivated(const QString &query);
    /// Double-clicked kanji anywhere in the pane (raw char — the
    /// opener filters through kanji info).
    void kanjiDoubleClicked(const QString &ch);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void clearContent();
    /// Meaning labels in layout order (objectName articleSense).
    QList<RunLabel *> senseLabels() const;
    /// Push one span edge (clears when the label is null).
    static void setRange(RunLabel *label, int anchor, int pos);
    /// Extend or collapse the press-drag span from a move event.
    /// `at` lives in `watched` coordinates.
    void updateSpan(QObject *watched, const QPoint &at);

    QWidget *content = nullptr;
    /// Press-drag span across meaning labels (null anchor = no
    /// gesture). Labels maintain single-label state themselves;
    /// the view only coordinates across them.
    QPointer<QWidget> spanAnchor;
    int spanAnchorOff = -1;
    bool spanning = false;
    bool hasSpan = false;
};
