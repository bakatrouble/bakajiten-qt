#pragma once

#include <QSize>

#include <cstdint>

/// Typed access to `state.yaml` (window state, not settings-window
/// content): the single home for memorized UI values — results
/// column widths, article width, panel size, kanji strip open
/// state. Same YAML storage as `AppSettings`, separate file next
/// to the settings (never user-edited; `BAKAJITEN_SETTINGS` redirects
/// it for test isolation). Key strings live here alone; callers
/// speak values.
class MemorizedState {
public:
    /// Which kanji-dialog strip (one open-state flag each).
    enum class KanjiStrip : std::uint8_t { Similar, Parts, PartOf };

    /// Fixed results-column widths, 0 when never memorized.
    static int resultsHeadingWidth();
    static void setResultsHeadingWidth(int width);
    static int resultsReadingWidth();
    static void setResultsReadingWidth(int width);
    /// Article pane width, 0 when never memorized.
    static int articleWidth();
    static void setArticleWidth(int width);
    /// Forget the article width (tests restoring the default).
    static void clearArticleWidth();
    /// All memorized absolute-pixel widths (columns and the
    /// article pane alike) rescaled once; identity is a no-op
    /// (never rewrites user values).
    static void rescaleWidths(double factor);
    /// Recognizer panel size (invalid when never memorized).
    static QSize panelSize();
    static void setPanelSize(const QSize &size);
    /// Kanji-dialog strip open state.
    static bool kanjiStripOpen(KanjiStrip strip);
    static void setKanjiStripOpen(KanjiStrip strip, bool open);
    /// Update version the user asked not to be notified about
    /// (empty = notify about everything newer).
    static QString skippedUpdateVersion();
    static void setSkippedUpdateVersion(const QString &version);
};
