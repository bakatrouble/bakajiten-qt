#pragma once

#include <QElapsedTimer>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QWidget>

#include <functional>

/// Kanji stroke-order diagram: KanjiVG polylines (pen space 0..255)
/// as light round strokes over the fixed dark drawing surface
/// (recognizer-canvas roles, both themes), with numbered badges at
/// stroke starts. Badge centers avoid each other (overlapping
/// starts, like strokes 1 and 5 of 我, nudge apart) instead of
/// following strict per-direction slots. All ink resolves per
/// paint, so theme switches need no rebuild; badge fills use the
/// selection pair ramp (guaranteed contrast on the dark ground).
class KanjiStrokeView : public QWidget {
    Q_OBJECT
public:
    explicit KanjiStrokeView(QWidget *parent = nullptr);

    void setStrokes(const QVector<QVector<QPointF>> &strokes);
    int strokeCount() const { return static_cast<int>(paths.size()); }
    /// Fully shown strokes (playback progress).
    int shownCount() const { return shown; }
    /// Badge squares for shown strokes, in stroke order (null for
    /// strokeless entries — numbering still follows the index).
    /// Paint draws these; tests prove overlap separation.
    QVector<QRectF> badgeRects() const;

    QSize sizeHint() const override;

public slots:
    /// Restart from nothing, playing.
    void restart();
    /// Resume playing (replays from the start at the end).
    void play();
    /// Freeze the current progress.
    void pause();
    /// Full diagram, stopped.
    void stop();
    /// Step one stroke (pauses).
    void stepForward();
    /// Step one stroke back (pauses).
    void stepBack();
    /// Playback speed level 1..10 (slow..fast); one stroke takes
    /// about a second at 1, a tenth of that at 10. Repaints run on
    /// the desktop frame clock so motion stays smooth at any speed.
    void setSpeed(int level);

signals:
    /// Progress for the counter (`shown` of `total`).
    void progressChanged(int shown, int total);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void advance();

private:
    /// Milliseconds per stroke at the current speed level.
    double strokeMs() const { return 1100.0 - 100.0 * speedLevel; }
    /// One frame at the desktop refresh rate (vsync-shaped motion);
    /// 60Hz fallback for headless/unknown screens.
    int frameMs() const;
    /// Mapped sub-polyline covering `fraction` of a stroke by arc
    /// length.
    static QPolygonF subStroke(const QVector<QPointF> &stroke, double fraction,
                               const std::function<QPointF(const QPointF &)> &map);

private:
    /// Pen-space bounding box of all strokes (empty when strokeless).
    QRectF bounds() const;
    /// Pen-space fit: origin plus scale into the fitted square.
    struct Fit {
        QPointF origin;
        double scale = 1.0;
        double cell = 1.0;
        QRectF box;
    };
    Fit fitBox(double cell) const;
    QPointF mapPoint(const QPointF &p, const Fit &fit) const;

    QVector<QVector<QPointF>> paths;
    /// Fully shown strokes plus the fraction of the next one.
    int shown = 0;
    double partial = 0.0;
    int speedLevel = 5;
    QTimer *playTimer = nullptr;
    /// Last tick time (wall-clock fractions in `advance`).
    QElapsedTimer lastTick;
};
