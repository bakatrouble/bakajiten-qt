#include "widgets/list/CandidateList.h"

#include "widgets/edgefade/EdgeFade.h"
#include "widgets/list/CandidateListDelegate.h"

#include <QAbstractItemView>
#include <QAbstractSlider>
#include <QApplication>
#include <QCursor>
#include <QEasingCurve>
#include <QEvent>
#include <QFont>
#include <QFontMetricsF>
#include <QListView>
#include <QListWidget>
#include <QMouseEvent>
#include <QObject>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSizePolicy>
#include <QVector>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>
#include <QtGlobal>

CandidateList::CandidateList(QWidget *parent) : QListWidget(parent) {
    setItemDelegate(new CandidateListDelegate(this));
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    // Horizontal strip contract (was split with the panel .ui —
    // every instance strips, wherever it is constructed).
    setFlow(QListView::LeftToRight);
    setWrapping(false);
    setMovement(QListView::Free);
    setUniformItemSizes(true);
    setGridSize(QSize(40, 40));
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDragDropMode(QAbstractItemView::DragOnly);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(40);
    setMaximumHeight(40);
    leftFade = new EdgeFade(EdgeFade::Side::Left, this);
    rightFade = new EdgeFade(EdgeFade::Side::Right, this);
    leftFade->setObjectName("leftFade");
    rightFade->setObjectName("rightFade");
    connect(horizontalScrollBar(), &QAbstractSlider::valueChanged, this,
            &CandidateList::updateFades);
    connect(horizontalScrollBar(), &QAbstractSlider::rangeChanged, this,
            &CandidateList::updateFades);
    viewport()->installEventFilter(this);
    setLayoutDirection(Qt::LeftToRight);
    updateFades();
}

void CandidateList::setHits(const QVector<CandidateHit> &hits) {
    clear();
    if (hits.isEmpty()) {
        addItem(tr("Draw a stroke first."));
        return;
    }
    // One cell per candidate, reading left to right; score on hover.
    // Coloring is fully delegated (script tones, hover invert), so
    // items carry no brushes of their own. The score rides along
    // as item data for the native tooltip; kanji cards resolve
    // window-wide (see KanjiHover).
    for (const CandidateHit &hit : hits) {
        auto *item = new QListWidgetItem(hit.text, this);
        item->setTextAlignment(Qt::AlignCenter);
        item->setToolTip(tr("score %1").arg(hit.score));
        item->setData(Qt::UserRole, QVariant::fromValue(hit.score));
    }
}

void CandidateList::refreshScale() {
    // Interface font at the current scale (never the list's own:
    // it is explicit after the first call, so re-deriving from it
    // would compound 2x into 4x on every live font change).
    QFont listFont = QApplication::font();
    if (listFont.pointSizeF() > 0.0) {
        listFont.setPointSizeF(listFont.pointSizeF() * 2.0);
    }
    setFont(listFont);
    const int row =
        qRound(QFontMetricsF(listFont).height() * CandidateListDelegate::cellSideFactor);
    setMinimumHeight(row);
    setMaximumHeight(row);
}

void CandidateList::refreshGrid() {
    const int cell = maximumViewportSize().height();
    setGridSize(QSize(cell, cell));
}

void CandidateList::smoothScrollStrip(int sideways) {
    auto *bar = horizontalScrollBar();
    if (stripScroll == nullptr) {
        stripScroll = new QPropertyAnimation(bar, "value", this);
        stripScroll->setDuration(180);
        stripScroll->setEasingCurve(QEasingCurve::OutCubic);
    }
    if (stripScroll->state() != QPropertyAnimation::Running) {
        stripTarget = bar->value();
    }
    stripTarget = qBound(bar->minimum(), stripTarget - sideways, bar->maximum());
    stripScroll->stop();
    stripScroll->setStartValue(bar->value());
    stripScroll->setEndValue(stripTarget);
    stripScroll->start();
}

void CandidateList::updateFades() {
    auto *bar = horizontalScrollBar();
    leftFade->setVisible(bar->value() > 0);
    rightFade->setVisible(bar->value() < bar->maximum());
    static constexpr int fadeWidth = 28;
    const QRect area = viewport()->geometry();
    leftFade->setGeometry(area.left(), area.top(), fadeWidth, area.height());
    rightFade->setGeometry(area.right() - fadeWidth + 1, area.top(), fadeWidth, area.height());
}

void CandidateList::resizeEvent(QResizeEvent *event) {
    QListWidget::resizeEvent(event);
    updateFades();
}

bool CandidateList::eventFilter(QObject *watched, QEvent *event) {
    if (watched == viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            const auto *me = static_cast<const QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                // Drag-scroll anchor; a plain click (no drag) still
                // falls through to the list, so it clicks through.
                stripDragging = false;
                stripPressPos = me->position().toPoint();
                auto *bar = horizontalScrollBar();
                stripPressValue = bar->value();
                if (stripScroll != nullptr) {
                    stripScroll->stop();
                }
                return false;
            }
        } else if (event->type() == QEvent::MouseMove) {
            const auto *me = static_cast<const QMouseEvent *>(event);
            if ((me->buttons() & Qt::LeftButton) != 0) {
                const QPoint delta = me->position().toPoint() - stripPressPos;
                if (!stripDragging &&
                    delta.manhattanLength() >= QApplication::startDragDistance()) {
                    stripDragging = true;
                    viewport()->setCursor(Qt::ClosedHandCursor);
                }
                if (stripDragging) {
                    // Content follows the cursor; consumed, so the
                    // release can't click.
                    auto *bar = horizontalScrollBar();
                    bar->setValue(stripPressValue - delta.x());
                    return true;
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            const auto *me = static_cast<const QMouseEvent *>(event);
            if (stripDragging && me->button() == Qt::LeftButton) {
                stripDragging = false;
                viewport()->unsetCursor();
                return true;
            }
        } else if (event->type() == QEvent::Wheel) {
            // Single-row strip with both scroll bars hidden: vertical
            // wheel glides the strip horizontally instead (animated, so
            // content never jumps — or moves vertically). Kanji cards
            // resolve window-wide (see KanjiHover).
            const auto *we = static_cast<const QWheelEvent *>(event);
            const int sideways =
                we->pixelDelta().x() != 0
                    ? we->pixelDelta().x()
                    : (we->angleDelta().x() != 0 ? we->angleDelta().x() : we->angleDelta().y());
            smoothScrollStrip(sideways);
            return true;
        }
    }
    return QListWidget::eventFilter(watched, event);
}
