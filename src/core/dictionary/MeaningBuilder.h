#pragma once

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPalette>
#include <QString>
#include <QVector>

#include <cstdint>

struct SenseDisplay;

/// Meaning runs builder (extracted from the results delegate):
/// zkanji-ordered runs with resolved pens, over one palette
/// snapshot. Paint, hover, and drag-select share these, so
/// selection math and drawn glyphs always agree.
class MeaningBuilder {
public:
    enum class Kind : std::uint8_t { Infl, Number, Pos, Misc, Uk, Gloss, Field, Gap };
    struct Run {
        QString text;
        QFont font;
        Kind kind;
        QColor pen;
        /// Native hover tooltip (tag descriptions, one per line;
        /// empty on runs without one).
        QString tip;
    };

    /// Runs for structured senses (empty only when no senses).
    /// Every sense is numbered, even a lone one (the article
    /// numbers single senses too — the row matches it). A
    /// non-empty inflection name (deinflected rows: `past`,
    /// `-tara conditional`, … — zkanji shows it ahead of the
    /// definitions) leads as its own small run. `selected`
    /// collapses every pen to the highlighted text.
    static QVector<Run> build(const QVector<SenseDisplay> &senses, const QFont &base,
                              const QString &inflection, const QPalette &palette, bool selected);

    /// Laid-out runs for one cell: starting x plus the drawn
    /// (right-elided) text per run. Paint, hover, and drag-select
    /// share this, so hit-testing can never drift from the glyphs
    /// once a row overflows (full-width walks would).
    struct PlacedRun {
        int x = 0;
        QString shown;
    };
    /// Horizontal extent a row lays out into: text left edge to the
    /// cell's right edge (paint may use the right margin).
    struct RowExtent {
        int left = 0;
        int right = 0;
    };
    static QVector<PlacedRun> layoutRuns(const QVector<Run> &runs, RowExtent extent);

private:
    static QColor penFor(Kind kind, const QPalette &palette, bool selected);
};
