#pragma once

#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QString>
#include <QStyle>
#include <QStyleFactory>

#include <QtGlobal>

#include <cstdint>

/// Reading display script: kana as packed, or romaji in two schools
/// (see `util/Romaji.h` for the exact mapping).
enum class ReadingDisplay : std::uint8_t { Kana = 0, Hepburn = 1, Kunrei = 2 };

/// Interface color scheme: follow the system, or force dark/light.
/// Forced modes set the application palette outright (theme styles
/// that bring their own colors, like Kvantum themes, may still win
/// for their own surfaces); canvas paper and inks derive from the
/// live palette either way.
enum class ThemeMode : std::uint8_t { System = 0, Dark = 1, Light = 2 };

/// Persisted appearance + interface scale: the single home for
/// settings storage and loading. Storage is a YAML file
/// (`settings.yaml` under the platform config location,
/// `BAKAJITEN_SETTINGS` overrides the path outright — tests use that
/// for isolation), accessed through `QSettings` with a registered
/// YAML format: keys like `ui/scale` nest by slashes on both sides,
/// so the file mirrors the key structure. Plain data: construction
/// is cheap defaults (no disk); `fromStorage()` reads, `save()` to
/// write, `==` for dirty checks, one `shared()` live instance.
///
/// Application (font re-set, style creation) happens in `apply()`,
/// from the desktop base read per apply — never compounded, never
/// stale. Every control in the settings dialog applies on change
/// (no Apply); scale changes still recreate the window.
///
/// (Panel geometry still lives in `QSettings`: window state, not
/// settings-window content.)
class AppSettings {
public:
    /// Cheap defaults (no disk access).
    AppSettings();
    /// Read persisted values (or defaults). Disk access: use
    /// sparingly, never per-frame or per-keystroke.
    static AppSettings fromStorage();

    double scale() const { return m_scale; }
    void setScale(double scale) { m_scale = qBound(0.5, scale, 3.0); }

    /// Widget style name (empty = OS default).
    QString style() const { return m_style; }
    void setStyle(const QString &style) { m_style = style; }

    ReadingDisplay reading() const { return m_reading; }
    void setReading(ReadingDisplay reading) { m_reading = reading; }

    ThemeMode mode() const { return m_mode; }
    void setMode(ThemeMode mode) { m_mode = mode; }

    /// Automatic update checks (startup + hourly). Off opts out.
    bool checkUpdates() const { return m_checkUpdates; }
    void setCheckUpdates(bool check) { m_checkUpdates = check; }

    bool operator==(const AppSettings &other) const;
    bool operator!=(const AppSettings &other) const { return !(*this == other); }

    /// Persist all values.
    void save() const;
    /// Re-read all values from storage.
    void load();

    /// Apply scale (app font from the desktop base) + style.
    void apply() const;
    /// Scale a base font by a factor, preserving point/pixel sizing
    /// (pure helper: pointSizeF() is -1 on pixel-sized fonts, where
    /// scaling points silently never applies).
    static QFont scaledFont(const QFont &base, double scale);
    /// Apply the style only: live-safe, no recreation needed.
    void applyStyle() const;
    /// Apply the color scheme (system palette, or the forced
    /// dark/light one): live-safe, repaints follow.
    void applyPalette() const;
    /// Palette for System mode: the chosen style's own standard
    /// palette, else the startup OS capture.
    QPalette systemPalette() const;
    /// Canvas paper follows the theme (dark palette = dark paper).
    bool darkPaper() const;

    /// Live process values.
    static AppSettings &shared();
    /// Storage file path (override or platform config location).
    static QString storagePath();
    /// Memorized UI values (panel size et al.): same YAML storage,
    /// separate file next to the settings (never user-edited).
    static QString memorizedPath();
    /// Delete the storage file (test isolation).
    static void resetStorage();
    /// Capture the pre-override style (the OS configuration) —
    /// call in main() before the persisted style applies.
    static void noteOsStyle();
    /// Capture the pre-override application palette — call in
    /// main() before anything applies (System mode restores it).
    static void noteOsPalette();
    /// OS-default style name for the Default entry (live style
    /// when uncaptured, e.g. in tests).
    static QString osStyleName();

private:
    double m_scale = 1.0;
    QString m_style;
    ReadingDisplay m_reading = ReadingDisplay::Kana;
    ThemeMode m_mode = ThemeMode::System;
    bool m_checkUpdates = true;
};
