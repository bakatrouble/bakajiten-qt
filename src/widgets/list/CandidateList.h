#pragma once

#include <QListWidget>
#include <QPoint>
#include <QString>
#include <QVector>

class EdgeFade;
class KanjiInfo;
class KanjiTooltip;
class QPropertyAnimation;

/// One candidate cell's worth of data (plain value at the boundary:
/// matching stays in RecognizerWidget, this widget only shows).
struct CandidateHit {
    QString text;
    double score = 0.0;
};

/// Horizontal candidate strip: font-following square cells, animated
/// wheel/drag scrolling (vertical wheel drives it, never scrolls),
/// edge fades, and plain-click-vs-drag disambiguation. Matching,
/// tooltips, and acceptance stay in RecognizerWidget; this widget
/// only owns list behavior (the .ui promotes candidatesList to it).
class CandidateList : public QListWidget {
    Q_OBJECT
public:
    explicit CandidateList(QWidget *parent = nullptr);

    /// Replace all cells (empty = placeholder row, no scores).
    void setHits(const QVector<CandidateHit> &hits);
    /// Rows follow the interface font (runs on build + FontChange).
    void refreshScale();
    /// Square cells fitted to the live viewport (post-layout only).
    void refreshGrid();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    /// Ease the bar toward the retargeted goal (wheel input).
    void smoothScrollStrip(int sideways);
    /// Show each fade exactly while its edge can scroll further.
    void updateFades();

    EdgeFade *leftFade = nullptr;
    EdgeFade *rightFade = nullptr;
    QPropertyAnimation *stripScroll = nullptr;
    int stripTarget = 0;
    /// Drag-scroll anchor; below the drag threshold the release
    /// still clicks through to the list.
    bool stripDragging = false;
    QPoint stripPressPos;
    int stripPressValue = 0;
};
