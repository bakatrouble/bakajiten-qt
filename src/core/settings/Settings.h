#pragma once

#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QString>
#include <QStyle>
#include <QStyleFactory>

#include <QtGlobal>

#include <cstdint>

/// Which icon set to draw (the canvas is the only consumer: it
/// flips its drawing surface while the accent stroke stays
/// constant). `Auto` derives from the theme (dark window color
/// means light icons and vice versa).
enum class IconScheme : std::uint8_t { Auto = 0, Light = 1, Dark = 2 };

/// Reading display script: kana as packed, or romaji in two schools
/// (see `util/Romaji.h` for the exact mapping).
enum class ReadingDisplay : std::uint8_t { Kana = 0, Hepburn = 1, Kunrei = 2 };

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
/// stale. Scale changes take effect on settings accept (the window
/// is recreated); theme + icons apply live.
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

    IconScheme icons() const { return m_icons; }
    void setIcons(IconScheme icons) { m_icons = icons; }

    ReadingDisplay reading() const { return m_reading; }
    void setReading(ReadingDisplay reading) { m_reading = reading; }

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
    /// Effective icon scheme (`Auto` resolved via the theme).
    IconScheme resolvedIcons() const;

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
    /// OS-default style name for the Default entry (live style
    /// when uncaptured, e.g. in tests).
    static QString osStyleName();

private:
    double m_scale = 1.0;
    QString m_style;
    IconScheme m_icons = IconScheme::Auto;
    ReadingDisplay m_reading = ReadingDisplay::Kana;
    bool m_checkUpdates = true;
};
