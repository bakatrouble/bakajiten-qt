#include "core/dictionary/MeaningBuilder.h"

#include "core/dictionary/DictionaryModel.h"
#include "core/palette/MaterialPalette.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPalette>
#include <QString>
#include <QVector>
#include <Qt>
#include <QtGlobal>

QVector<MeaningBuilder::Run> MeaningBuilder::build(const QVector<SenseDisplay> &senses,
                                                   const QFont &base, const QString &inflection,
                                                   const QPalette &palette, bool selected) {
    QFont small = base;
    small.setPointSizeF(qMax(6.0, small.pointSizeF() - 2.0));
    QVector<Run> runs;
    // One run per tag (concatenated text identical to the old
    // joined run); each run carries only its own description.
    const auto appendTags = [&](const QStringList &labels, const QStringList &tips, Kind kind,
                                const QString &lead, const QString &trail) {
        for (int i = 0; i < labels.size(); ++i) {
            const QString text = (i == 0 ? lead : QString()) + labels[i] +
                                 (i + 1 < labels.size() ? QStringLiteral(", ") : trail);
            const QString tip = i < tips.size() ? tips[i] : QString();
            runs << Run{text, small, kind, penFor(kind, palette, selected), tip};
        }
    };
    if (!inflection.isEmpty()) {
        runs << Run{inflection + QStringLiteral(" "),
                    small,
                    Kind::Infl,
                    penFor(Kind::Infl, palette, selected),
                    {}};
    }
    int n = 0;
    for (const SenseDisplay &sense : senses) {
        ++n;
        runs << Run{QStringLiteral("%1. ").arg(n),
                    base,
                    Kind::Number,
                    penFor(Kind::Number, palette, selected),
                    {}};
        if (!sense.pos.isEmpty()) {
            appendTags(sense.pos, sense.posTips, Kind::Pos, QString(), QStringLiteral(" "));
        }
        if (!sense.misc.isEmpty()) {
            // Per-tag runs: only `uk` reads warning-red, the rest
            // stay dim. `uk` displays as `kana` (data, tooltips, and
            // kana-only detection keep the raw tag). Tips pair
            // one-to-one with labels (see SenseDisplay).
            for (int i = 0; i < sense.misc.size(); ++i) {
                const QString separator =
                    i + 1 < sense.misc.size() ? QStringLiteral(", ") : QStringLiteral(" ");
                const QString tip = i < sense.miscTips.size() ? sense.miscTips[i] : QString();
                const MeaningBuilder::Kind kind = isUsuallyKana(sense.misc[i])
                                                      ? MeaningBuilder::Kind::Uk
                                                      : MeaningBuilder::Kind::Misc;
                const QString shown =
                    kind == MeaningBuilder::Kind::Uk ? QStringLiteral("kana") : sense.misc[i];
                runs << Run{shown + separator, small, kind, penFor(kind, palette, selected), tip};
            }
        }
        runs << Run{sense.gloss, base, Kind::Gloss, penFor(Kind::Gloss, palette, selected), {}};
        if (!sense.field.isEmpty()) {
            appendTags(sense.field, sense.fieldTips, Kind::Field, QStringLiteral(" "), QString());
        }
        runs << Run{
            QStringLiteral("  "), base, Kind::Gap, penFor(Kind::Gap, palette, selected), {}};
    }
    return runs;
}

QVector<MeaningBuilder::PlacedRun> MeaningBuilder::layoutRuns(const QVector<Run> &runs,
                                                              RowExtent extent) {
    // Same walk the delegate paints: each run ellipsized against
    // the remaining width, advances by the drawn text.
    QVector<PlacedRun> placed;
    int x = extent.left;
    const int right = extent.right;
    for (const Run &run : runs) {
        const QFontMetrics runMetrics(run.font);
        placed << PlacedRun{x, runMetrics.elidedText(run.text, Qt::ElideRight, right - x + 1)};
        x += runMetrics.horizontalAdvance(placed.last().shown);
        if (x > right) {
            break;
        }
    }
    return placed;
}

QColor MeaningBuilder::penFor(Kind kind, const QPalette &palette, bool selected) {
    // Inflection names read as tags (small green, like pos).
    // Selected rows paint every run in the highlighted-text pen.
    const MaterialPalette theme = MaterialPalette::fromPalette(palette);
    if (selected) {
        return theme.highlightedText();
    }
    if (kind == Kind::Pos || kind == Kind::Infl) {
        return theme.accentGreen();
    }
    if (kind == Kind::Misc) {
        return theme.dimmed();
    }
    if (kind == Kind::Uk) {
        return theme.warning();
    }
    if (kind == Kind::Field) {
        return theme.link();
    }
    return theme.text();
}
