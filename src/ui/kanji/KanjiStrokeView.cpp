#include "ui/kanji/KanjiStrokeView.h"

#include "core/palette/MaterialPalette.h"

#include <QPaintEvent>
#include <QPainter>
#include <QScreen>
#include <QTimer>

#include <cmath>

namespace {
/// Badge diameter and stroke width relative to the diagram cell.
constexpr double badgeFraction = 0.075;
constexpr double strokeFraction = 0.035;
/// Fixed dark drawing surface (recognizer-canvas roles at their
/// dark variant, both themes — the badges and light ink need the
/// contrast everywhere).
constexpr bool darkPaper = true;
/// Badge gap as a fraction of the badge diameter: neighbors may
/// touch, never cover.
constexpr double gapFraction = 0.1;
/// Catch-up clamp in frames: a stalled tick resumes smoothly
/// instead of jumping whole strokes.
constexpr double maxCatchUpFrames = 4.0;
} // namespace

KanjiStrokeView::KanjiStrokeView(QWidget *parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    playTimer = new QTimer(this);
    connect(playTimer, &QTimer::timeout, this, &KanjiStrokeView::advance);
}

void KanjiStrokeView::setStrokes(const QVector<QVector<QPointF>> &strokes) {
    paths = strokes;
    stop();
    updateGeometry();
    update();
}

void KanjiStrokeView::play() {
    if (paths.isEmpty()) {
        return;
    }
    // At the end: replay from the start, like the mock's restart.
    if (shown >= static_cast<int>(paths.size())) {
        shown = 0;
        partial = 0.0;
    }
    // Frame-rate tick (re-read per play: cheap, tracks monitor
    // changes between plays); wall-clock fractions below measure
    // real elapsed time, so throttled timers stay accurate.
    playTimer->setInterval(frameMs());
    lastTick.start();
    playTimer->start();
    emit progressChanged(shown, strokeCount());
}

void KanjiStrokeView::restart() {
    shown = 0;
    partial = 0.0;
    play();
}

void KanjiStrokeView::pause() {
    playTimer->stop();
    emit progressChanged(shown, strokeCount());
}

void KanjiStrokeView::stop() {
    playTimer->stop();
    shown = static_cast<int>(paths.size());
    partial = 0.0;
    emit progressChanged(shown, strokeCount());
    update();
}

void KanjiStrokeView::stepForward() {
    playTimer->stop();
    shown = qMin(shown + 1, static_cast<int>(paths.size()));
    partial = 0.0;
    emit progressChanged(shown, strokeCount());
    update();
}

void KanjiStrokeView::stepBack() {
    playTimer->stop();
    shown = qMax(shown - 1, 0);
    partial = 0.0;
    emit progressChanged(shown, strokeCount());
    update();
}

void KanjiStrokeView::setSpeed(int level) { speedLevel = qBound(1, level, 10); }

int KanjiStrokeView::frameMs() const {
    const QScreen *screen = this->screen();
    const qreal rate = screen != nullptr ? screen->refreshRate() : 0.0;
    if (rate > 1.0) {
        return qMax(1, qRound(1000.0 / rate));
    }
    return 17;
}

void KanjiStrokeView::advance() {
    if (paths.isEmpty() || shown >= static_cast<int>(paths.size())) {
        playTimer->stop();
        return;
    }
    // Measured wall-clock fraction per tick: motion speed is
    // constant along the stroke (see the arc-length partial below)
    // and independent of point density and timer jitter. Clamped
    // past stalls so occluded windows resume instead of jumping.
    // Synchronously invoked advances (no clock running) count one
    // frame each, deterministically.
    qint64 elapsed = frameMs();
    if (lastTick.isValid()) {
        elapsed = lastTick.restart();
    } else {
        lastTick.start();
    }
    const double frame = static_cast<double>(frameMs());
    partial += qMin(static_cast<double>(elapsed), maxCatchUpFrames * frame) / strokeMs();
    if (partial >= 1.0) {
        partial = 0.0;
        ++shown;
        if (shown >= static_cast<int>(paths.size())) {
            playTimer->stop();
        }
    }
    emit progressChanged(shown, strokeCount());
    update();
}

QSize KanjiStrokeView::sizeHint() const { return {260, 300}; }

QRectF KanjiStrokeView::bounds() const {
    // Manual min/max: uniting onto a null QRectF leaves it
    // degenerate (probe-verified), so never seed from one.
    bool any = false;
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
    for (const auto &stroke : paths) {
        for (const QPointF &p : stroke) {
            if (!any) {
                x0 = x1 = p.x();
                y0 = y1 = p.y();
                any = true;
            } else {
                x0 = qMin(x0, p.x());
                y0 = qMin(y0, p.y());
                x1 = qMax(x1, p.x());
                y1 = qMax(y1, p.y());
            }
        }
    }
    return any ? QRectF(QPointF(x0, y0), QPointF(x1, y1)) : QRectF();
}

void KanjiStrokeView::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const MaterialPalette theme = MaterialPalette::fromPalette(palette());
    painter.fillRect(rect(), MaterialPalette::canvasPaper(darkPaper));
    // Fit the ink with room for the offset badges, kept square.
    const double badge = qMax(12.0, qMin(width(), height()) * badgeFraction);
    const double margin = badge * 1.1;
    const double cell = qMin(width(), height()) - 2.0 * margin;
    const Fit fit = fitBox(qMax(1.0, cell));
    // Grid over the fitted square.
    const QRectF fitted(fit.origin, QSizeF(fit.cell, fit.cell));
    painter.setPen(MaterialPalette::canvasGrid(darkPaper));
    painter.drawRect(fitted);
    for (int i = 1; i < 4; ++i) {
        const double t = fitted.left() + fit.cell * i / 4.0;
        const double u = fitted.top() + fit.cell * i / 4.0;
        painter.drawLine(QPointF(t, fitted.top()), QPointF(t, fitted.bottom()));
        painter.drawLine(QPointF(fitted.left(), u), QPointF(fitted.right(), u));
    }
    // Playback progress: completed strokes fully, the next one
    // fractionally, badges for completed strokes only.
    QPen ink(MaterialPalette::canvasInk(darkPaper));
    ink.setWidthF(qMax(3.0, qMin(width(), height()) * strokeFraction));
    ink.setCapStyle(Qt::RoundCap);
    ink.setJoinStyle(Qt::RoundJoin);
    painter.setPen(ink);
    const int done = qBound(0, shown, static_cast<int>(paths.size()));
    for (int s = 0; s < done; ++s) {
        QPolygonF poly;
        for (const QPointF &p : paths[s]) {
            poly << mapPoint(p, fit);
        }
        painter.drawPolyline(poly);
    }
    if (partial > 0.0 && done < static_cast<int>(paths.size())) {
        const QPolygonF poly =
            subStroke(paths[done], partial, [&](const QPointF &p) { return mapPoint(p, fit); });
        if (poly.size() > 1) {
            painter.drawPolyline(poly);
        }
    }
    // Numbered badges at stroke starts (see badgeRects for the
    // overlap-free placement), each with a pointer triangle into
    // its start point. Fills ramp blue → green by number.
    QFont badgeFont = font();
    badgeFont.setBold(true);
    badgeFont.setPointSizeF(qMax(7.0, badge * 0.55));
    painter.setFont(badgeFont);
    painter.setPen(Qt::NoPen);
    const QVector<QRectF> badges = badgeRects();
    for (int i = 0; i < badges.size() && i < static_cast<int>(paths.size()); ++i) {
        const QRectF &rect = badges[i];
        if (rect.isNull() || paths[i].isEmpty()) {
            continue;
        }
        const QPointF center = rect.center();
        const QPointF start = mapPoint(paths[i].first(), fit);
        const QColor fill = theme.strokeBadge(i, static_cast<int>(paths.size()));
        painter.setBrush(fill);
        painter.drawRoundedRect(rect, badge / 3.0, badge / 3.0);
        // Pointer triangle over the badge, apex at the stroke
        // start: reads as an arrow even at small sizes.
        const QPointF dir = center - start;
        const double dist = qMax(1.0, std::hypot(dir.x(), dir.y()));
        const QPointF along(dir.x() / dist, dir.y() / dist);
        const QPointF across(-along.y(), along.x());
        const QPointF base = start + along * badge * 0.55;
        const double half = badge * 0.24;
        painter.drawPolygon(QPolygonF({start, base + across * half, base - across * half}));
        painter.setPen(Qt::white);
        painter.drawText(rect, Qt::AlignCenter, QString::number(i + 1));
        painter.setPen(Qt::NoPen);
    }
}

KanjiStrokeView::Fit KanjiStrokeView::fitBox(double cell) const {
    const QRectF box = bounds();
    const double span = qMax(box.width(), box.height());
    Fit fit;
    fit.cell = cell;
    fit.box = box;
    fit.scale = span > 0.0 ? cell / span : 1.0;
    fit.origin = QPointF((width() - cell) / 2.0, (height() - cell) / 2.0);
    return fit;
}

QPointF KanjiStrokeView::mapPoint(const QPointF &p, const Fit &fit) const {
    return fit.origin + (p - fit.box.topLeft()) * fit.scale;
}

/// Default badge direction for one stroke: left of vertical
/// strokes, top-right of horizontal ones (dots and degenerate
/// strokes read as horizontal, badge above). The placer tries this
/// first, so calm diagrams keep the familiar layout.
static QPointF defaultBadgeDir(const QPointF &delta) {
    if (delta.manhattanLength() < 1.0 || qAbs(delta.x()) >= qAbs(delta.y())) {
        return {0.7071, -0.7071};
    }
    return {-1.0, 0.0};
}

QVector<QRectF> KanjiStrokeView::badgeRects() const {
    QVector<QRectF> out;
    const double badge = qMax(12.0, qMin(width(), height()) * badgeFraction);
    const double margin = badge * 1.1;
    const double cell = qMax(1.0, qMin(width(), height()) - 2.0 * margin);
    const Fit fit = fitBox(cell);
    const int done = qBound(0, shown, static_cast<int>(paths.size()));
    const double gap = badge * gapFraction;
    for (int i = 0; i < done; ++i) {
        if (paths[i].isEmpty()) {
            out << QRectF();
            continue;
        }
        const QPointF start = mapPoint(paths[i].first(), fit);
        const QPointF end = mapPoint(paths[i].last(), fit);
        // Compass ring past the default slot: first free rect
        // wins, so colliding starts (strokes 1 and 5 of 我) nudge
        // apart instead of stacking.
        QRectF placed;
        bool found = false;
        auto candidates = [&](double dist) {
            QVector<QPointF> out;
            out << defaultBadgeDir(end - start) * dist;
            for (const QPointF &compass :
                 {QPointF(1, 0), QPointF(0.7071, 0.7071), QPointF(0, 1), QPointF(-0.7071, 0.7071),
                  QPointF(-1, 0), QPointF(-0.7071, -0.7071), QPointF(0, -1),
                  QPointF(0.7071, -0.7071)}) {
                out << compass * dist;
            }
            return out;
        };
        for (double dist : {1.0, 1.9}) {
            for (const QPointF &dir : candidates(dist)) {
                QPointF center = start + dir * badge;
                // Keep the badge on-widget (strokes near the
                // fitted edge would otherwise push it out); the
                // pointer still reaches back to the start.
                center.setX(qBound(badge / 2.0, center.x(), width() - badge / 2.0));
                center.setY(qBound(badge / 2.0, center.y(), height() - badge / 2.0));
                const QRectF rect(center - QPointF(badge / 2.0, badge / 2.0), QSizeF(badge, badge));
                bool clash = false;
                for (const QRectF &taken : out) {
                    if (!taken.isNull() && taken.adjusted(-gap, -gap, gap, gap).intersects(rect)) {
                        clash = true;
                        break;
                    }
                }
                if (!clash) {
                    placed = rect;
                    found = true;
                    break;
                }
            }
            if (found) {
                break;
            }
        }
        if (!found) {
            // Saturated corner: fall back to the default slot
            // (overlap accepted, badge still on-widget).
            QPointF center = start + defaultBadgeDir(end - start) * badge;
            center.setX(qBound(badge / 2.0, center.x(), width() - badge / 2.0));
            center.setY(qBound(badge / 2.0, center.y(), height() - badge / 2.0));
            placed = QRectF(center - QPointF(badge / 2.0, badge / 2.0), QSizeF(badge, badge));
        }
        out << placed;
    }
    return out;
}

/// Mapped sub-polyline covering `fraction` of the stroke by arc
/// length (constant drawing speed regardless of point density).
QPolygonF KanjiStrokeView::subStroke(const QVector<QPointF> &stroke, double fraction,
                                     const std::function<QPointF(const QPointF &)> &map) {
    QPolygonF poly;
    if (stroke.size() < 2 || fraction <= 0.0) {
        return poly;
    }
    QVector<QPointF> mapped;
    mapped.reserve(stroke.size());
    for (const QPointF &p : stroke) {
        mapped << map(p);
    }
    double total = 0.0;
    for (int i = 1; i < mapped.size(); ++i) {
        total += std::hypot(mapped[i].x() - mapped[i - 1].x(), mapped[i].y() - mapped[i - 1].y());
    }
    if (total <= 0.0) {
        return poly;
    }
    double target = qBound(0.0, fraction, 1.0) * total;
    poly << mapped.first();
    for (int i = 1; i < mapped.size(); ++i) {
        const double seg =
            std::hypot(mapped[i].x() - mapped[i - 1].x(), mapped[i].y() - mapped[i - 1].y());
        if (target > seg) {
            target -= seg;
            poly << mapped[i];
        } else {
            const double t = seg > 0.0 ? target / seg : 0.0;
            poly << mapped[i - 1] + t * (mapped[i] - mapped[i - 1]);
            break;
        }
    }
    return poly;
}
