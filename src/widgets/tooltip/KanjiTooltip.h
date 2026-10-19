#pragma once

#include "core/dictionary/KanjiInfo.h"
#include "widgets/tooltip/Tooltip.h"

namespace Ui {
class KanjiTooltip;
}

/// Kanji hover card (see the mock): big glyph with Strokes/Radical
/// header, meanings, ON/Kun rows, Jouyou/JLPT footer. Same framed
/// card as the candidate tip; the base handles delays, placement,
/// and dismissal.
///
/// Structure lives in `KanjiTooltip.ui` (built via `buildForm` like
/// the kanji dialog); dynamic bits — scale-following fonts,
/// palette-driven section dimming, visibility — stay in `wireUi`
/// and `refresh` (no stylesheets: they bake at polish time and go
/// stale across theme switches).
class KanjiTooltip : public Tooltip {
    Q_OBJECT
public:
    explicit KanjiTooltip(QWidget *parent = nullptr);

    /// Hovered a kanji (structured card content rides along for the
    /// delayed show).
    void hover(const QString &ch, const KanjiInfo::Display &card);

protected:
    void refresh() override;

private:
    Ui::KanjiTooltip *ui = nullptr;
    QWidget *devForm = nullptr;
    KanjiInfo::Display pending;
};
