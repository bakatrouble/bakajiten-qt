#pragma once

#include <QColor>
#include <QPalette>
#include <QString>

/// Application palette: every color the UI builds, uses, or
/// adjusts, resolved for a theme through one object — call sites
/// own no hex, branch on no theme, and tweak no alpha. From a live
/// palette (`fromPalette`); canvas roles take their paper flag
/// explicitly instead (settings-driven, not palette-derived).
class MaterialPalette {
public:
    static MaterialPalette fromPalette(const QPalette &palette) { return MaterialPalette(palette); }
    /// Theme probe shared by settings (icon scheme) and the object
    /// itself: one threshold, one place.
    static bool isDarkPalette(const QPalette &palette) {
        return palette.color(QPalette::Window).lightness() < 128;
    }
    bool isDark() const { return dark; }

    /// Container background + the text color drawn on it.
    struct TonalSpot {
        QColor container;
        QColor onContainer;
    };

    /// Main text ink (selected rows read highlighted).
    QColor text(bool selected = false) const {
        return selected ? highlightedText() : palette.color(QPalette::Text);
    }
    /// Secondary ink for misc runs and readings.
    QColor dimmed(bool selected = false) const {
        return selected ? highlightedText() : palette.color(QPalette::PlaceholderText);
    }
    /// Link ink: fixed blue per theme. Style Link roles go
    /// near-white on dark themes (Breeze #b7c5f9, probe-verified),
    /// unreadable as links — the 足 "See also" terms looked plain
    /// white. Same hex-spec pattern as the accent green.
    QColor link(bool selected = false) const {
        return selected ? highlightedText() : QColor(linkBlueHex(dark));
    }
    /// Warning ink for the `uk` tag ("usually written using kana
    /// alone"): fixed red per theme, so the marker reads as a
    /// warning on every style.
    QColor warning(bool selected = false) const {
        return selected ? highlightedText() : QColor(warningRedHex(dark));
    }
    /// Selection and wash fill.
    QColor highlight() const { return palette.color(QPalette::Highlight); }
    QColor highlightedText() const { return palette.color(QPalette::HighlightedText); }
    /// Surfaces.
    QColor window() const { return palette.color(QPalette::Window); }
    QColor windowText() const { return palette.color(QPalette::WindowText); }
    QColor base() const { return palette.color(QPalette::Base); }
    QColor mid() const { return palette.color(QPalette::Mid); }

    /// App accent green (tags, matches, badges): readable ink on
    /// both themes.
    QColor accentGreen() const { return QColor(accentGreenHex(dark)); }
    /// Stroke-order badge fill for stroke `index` of `total`: a
    /// smooth blue → green hue ramp (first stroke blue, last green).
    /// Computed, theme-independent; white text stays readable on
    /// every step (medium-dark fills throughout).
    QColor strokeBadge(int index, int total) const {
        const float t =
            total > 1
                ? qBound(0.0f, static_cast<float>(index) / static_cast<float>(total - 1), 1.0f)
                : 0.0f;
        return QColor::fromHslF(0.62f + t * (0.33f - 0.62f), 0.70f, 0.42f);
    }
    /// Kana scripts (katakana first, like the strip): vivid
    /// containers with contrasting text, per theme.
    TonalSpot kanaSpot(bool katakana) const {
        return spot(katakana ? katakanaSeed() : hiraganaSeed(), dark);
    }
    /// Hover wash over unselected rows.
    QColor hoverWash() const {
        QColor wash = highlight();
        wash.setAlpha(32);
        return wash;
    }
    /// Deinflected-row tint under conjugated-form hits.
    QColor deinflectedTint() const {
        QColor tint = accentGreen();
        tint.setAlpha(28);
        return tint;
    }
    /// Kana-only headings: the text pen at slight transparency.
    QColor fadedText(bool selected = false) const {
        QColor faded = text(selected);
        faded.setAlpha(100);
        return faded;
    }
    /// Drag text-selection pair (inverted on selected rows: the row
    /// fill is already Highlight, so two bright colors would stack
    /// invisibly otherwise).
    QColor selectionFill(bool selected) const { return selected ? highlightedText() : highlight(); }
    QColor selectionInk(bool selected) const { return selected ? highlight() : highlightedText(); }
    /// Fade-to-background end stop (edge fades).
    QColor fadeEnd() const {
        QColor transparent = base();
        transparent.setAlpha(0);
        return transparent;
    }
    /// Handwriting panel card: filled background a touch above the
    /// window color, 1px border, grip ink.
    QColor cardFill() const { return window().lighter(108); }
    QColor cardBorder() const { return mid().lighter(120); }
    QColor cardGrip() const { return mid(); }

    /// Handwriting canvas roles over the neutral seed (`darkPaper`
    /// is the paper flag, not the palette theme — see the canvas
    /// call site).
    static QColor canvasPaper(bool darkPaper) {
        return tone(neutralSeed(), darkPaper ? 0.12F : 0.95F);
    }
    static QColor canvasGrid(bool darkPaper) {
        return tone(neutralSeed(), darkPaper ? 0.23F : 0.79F);
    }
    static QColor canvasHelp() { return tone(neutralSeed(), 0.54F); }
    static QColor canvasInk(bool darkPaper) {
        return tone(neutralSeed(), darkPaper ? 0.91F : 0.13F);
    }
    /// Active stroke: amber reads on both surfaces, stays constant.
    static QColor activeStroke() { return QColor(QStringLiteral("#FFC55E")); }

    /// Hex specifications — the single source of literal color.
    static QString accentGreenHex(bool dark) {
        return dark ? QStringLiteral("#81C47F") : QStringLiteral("#2E7D32");
    }
    static QString linkBlueHex(bool dark) {
        return dark ? QStringLiteral("#8AB4F8") : QStringLiteral("#0000FF");
    }
    static QString warningRedHex(bool dark) {
        return dark ? QStringLiteral("#E57373") : QStringLiteral("#C62828");
    }
    static QString katakanaSeedHex() { return QStringLiteral("#4285F4"); }
    static QString hiraganaSeedHex() { return QStringLiteral("#34A853"); }

    /// Seed with an explicit lightness (0-1), hue/saturation kept
    /// unless overridden.
    static QColor tone(const QColor &seed, float lightness, float saturation = -1.0F) {
        float hue = 0.0F, sat = 0.0F, light = 0.0F, alpha = 1.0F;
        seed.getHslF(&hue, &sat, &light, &alpha);
        return QColor::fromHslF(hue, saturation < 0.0F ? sat : saturation,
                                qBound(0.0F, lightness, 1.0F), alpha);
    }

    /// M3-ish roles: light theme gets a bright container with
    /// near-black text, dark theme a deep container with near-white.
    static TonalSpot spot(const QColor &seed, bool dark) {
        return dark ? TonalSpot{tone(seed, 0.30F), tone(seed, 0.92F, 0.10F)}
                    : TonalSpot{tone(seed, 0.88F), tone(seed, 0.12F, 0.15F)};
    }

    /// Elevation shadow/scrim: always black, themed only by opacity.
    static QColor scrim(float opacity) {
        QColor black(0, 0, 0);
        black.setAlphaF(qBound(0.0F, opacity, 1.0F));
        return black;
    }

private:
    explicit MaterialPalette(const QPalette &palette)
        : palette(palette), dark(isDarkPalette(palette)) {}
    static QColor katakanaSeed() { return QColor(katakanaSeedHex()); }
    static QColor hiraganaSeed() { return QColor(hiraganaSeedHex()); }
    static QColor neutralSeed() { return QColor(QStringLiteral("#808080")); }

    QPalette palette;
    bool dark = false;
};
