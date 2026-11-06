#include "Settings.h"

#include "core/palette/MaterialPalette.h"
#include "core/settings/YamlFormat.h"

#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLoggingCategory>
#include <QSettings>
#include <QStandardPaths>
#include <QString>

#include <algorithm>

Q_LOGGING_CATEGORY(settingsLog, "ui.settings");

namespace {
/// OS-default style captured at startup (before overrides).
QString &osStyleSlot() {
    static QString name;
    return name;
}
/// Application palette captured at startup (the style + platform
/// theme result, before any forced mode): System restores it.
QPalette &osPaletteSlot() {
    static QPalette palette;
    static bool captured = false;
    if (!captured) {
        palette = QApplication::palette();
        captured = true;
    }
    return palette;
}

/// Forced dark palette (GraphiteDark-like): every content role
/// covered so no text paints with a style default meant for
/// light. Disabled inks dim against the dark surface.
QPalette darkForcedPalette() {
    QPalette out;
    out.setColor(QPalette::Window, QColor(0x2c, 0x2c, 0x2c));
    out.setColor(QPalette::WindowText, QColor(0xdf, 0xdf, 0xdf));
    out.setColor(QPalette::Base, QColor(0x30, 0x30, 0x30));
    out.setColor(QPalette::AlternateBase, QColor(0x30, 0x30, 0x30));
    out.setColor(QPalette::Text, QColor(0xdf, 0xdf, 0xdf));
    out.setColor(QPalette::BrightText, QColor(0xff, 0xff, 0xff));
    out.setColor(QPalette::Button, QColor(0x47, 0x47, 0x47));
    out.setColor(QPalette::ButtonText, QColor(0xdf, 0xdf, 0xdf));
    out.setColor(QPalette::Light, QColor(0x44, 0x44, 0x44));
    out.setColor(QPalette::Midlight, QColor(0x39, 0x39, 0x39));
    out.setColor(QPalette::Dark, QColor(0x1a, 0x1a, 0x1a));
    out.setColor(QPalette::Mid, QColor(0x27, 0x27, 0x27));
    out.setColor(QPalette::Shadow, QColor(0x13, 0x13, 0x13));
    out.setColor(QPalette::Highlight, QColor(0x2c, 0x57, 0xa4));
    out.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    out.setColor(QPalette::Link, QColor(0xb7, 0xc5, 0xf9));
    out.setColor(QPalette::LinkVisited, QColor(0x00, 0xbc, 0xd4));
    // Tooltips read their own roles: leave them unset and they
    // fall through to the system scheme (dark here), so the card
    // would keep a dark background under a forced Light.
    out.setColor(QPalette::ToolTipBase, QColor(0x23, 0x23, 0x23));
    out.setColor(QPalette::ToolTipText, QColor(0xff, 0xff, 0xff));
    out.setColor(QPalette::Disabled, QPalette::ToolTipBase, QColor(0x23, 0x23, 0x23));
    out.setColor(QPalette::Disabled, QPalette::ToolTipText, QColor(0x80, 0x80, 0x80));
    out.setColor(QPalette::PlaceholderText, QColor(0x80, 0x80, 0x80));
    out.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x80, 0x80, 0x80));
    out.setColor(QPalette::Disabled, QPalette::Text, QColor(0x80, 0x80, 0x80));
    out.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x80, 0x80, 0x80));
    out.setColor(QPalette::Disabled, QPalette::Base, QColor(0x2a, 0x2a, 0x2a));
    out.setColor(QPalette::Disabled, QPalette::Button, QColor(0x3f, 0x3f, 0x3f));
    out.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0x1e, 0x3a, 0x5f));
    return out;
}

/// Forced light palette (hardcoded Fusion-light values — never
/// `QStyleFactory::create("fusion")->standardPalette()`: since Qt
/// 6.8 Fusion adapts its standard palette to the system color
/// scheme, which would hand a dark scheme right back on a dark
/// desktop).
QPalette lightForcedPalette() {
    QPalette out;
    out.setColor(QPalette::Window, QColor(0xef, 0xf0, 0xf1));
    out.setColor(QPalette::WindowText, QColor(0x30, 0x2f, 0x2f));
    out.setColor(QPalette::Base, QColor(0xff, 0xff, 0xff));
    out.setColor(QPalette::AlternateBase, QColor(0xef, 0xf0, 0xf1));
    out.setColor(QPalette::Text, QColor(0x30, 0x2f, 0x2f));
    out.setColor(QPalette::BrightText, QColor(0xff, 0xff, 0xff));
    out.setColor(QPalette::Button, QColor(0xef, 0xf0, 0xf1));
    out.setColor(QPalette::ButtonText, QColor(0x30, 0x2f, 0x2f));
    out.setColor(QPalette::Light, QColor(0xff, 0xff, 0xff));
    out.setColor(QPalette::Midlight, QColor(0xe0, 0xe0, 0xe0));
    out.setColor(QPalette::Dark, QColor(0x88, 0x88, 0x88));
    out.setColor(QPalette::Mid, QColor(0xb8, 0xb8, 0xb8));
    out.setColor(QPalette::Shadow, QColor(0x76, 0x76, 0x76));
    out.setColor(QPalette::Highlight, QColor(0x3d, 0xae, 0xe9));
    out.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    out.setColor(QPalette::Link, QColor(0x2a, 0x82, 0xda));
    out.setColor(QPalette::LinkVisited, QColor(0x8e, 0x44, 0xad));
    // Tooltips read their own roles: leave them unset and they
    // fall through to the system scheme, so pin them too.
    out.setColor(QPalette::ToolTipBase, QColor(0xff, 0xff, 0xdc));
    out.setColor(QPalette::ToolTipText, QColor(0x00, 0x00, 0x00));
    out.setColor(QPalette::Disabled, QPalette::ToolTipBase, QColor(0xff, 0xff, 0xdc));
    out.setColor(QPalette::Disabled, QPalette::ToolTipText, QColor(0x7f, 0x7f, 0x7f));
    out.setColor(QPalette::PlaceholderText, QColor(0x76, 0x76, 0x76));
    out.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x7f, 0x7f, 0x7f));
    out.setColor(QPalette::Disabled, QPalette::Text, QColor(0x7f, 0x7f, 0x7f));
    out.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x7f, 0x7f, 0x7f));
    out.setColor(QPalette::Disabled, QPalette::Base, QColor(0xf0, 0xf0, 0xf0));
    out.setColor(QPalette::Disabled, QPalette::Button, QColor(0xe9, 0xea, 0xeb));
    out.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0xa0, 0xc4, 0xe3));
    return out;
}
} // namespace

AppSettings::AppSettings() = default;

AppSettings AppSettings::fromStorage() {
    AppSettings stored;
    stored.load();
    return stored;
}

bool AppSettings::operator==(const AppSettings &other) const {
    return qFuzzyCompare(m_scale + 1.0, other.m_scale + 1.0) && m_style == other.m_style &&
           m_reading == other.m_reading && m_mode == other.m_mode &&
           m_checkUpdates == other.m_checkUpdates;
}

void AppSettings::save() const {
    QSettings storage(AppSettings::storagePath(), yamlFormat());
    storage.setValue(QStringLiteral("ui/scale"), m_scale);
    storage.setValue(QStringLiteral("ui/style"), m_style);
    storage.setValue(QStringLiteral("ui/reading"), static_cast<int>(m_reading));
    storage.setValue(QStringLiteral("ui/mode"), static_cast<int>(m_mode));
    storage.setValue(QStringLiteral("ui/checkUpdates"), m_checkUpdates);
    // Dropped keys stay behind in old files otherwise; the icons
    // theme was removed (canvas paper follows the theme now).
    storage.remove(QStringLiteral("ui/icons"));
    storage.remove(QStringLiteral("icons"));
    storage.sync();
}

void AppSettings::load() {
    // Defaults first; missing keys keep them.
    m_scale = 1.0;
    m_style.clear();
    m_reading = ReadingDisplay::Kana;
    m_mode = ThemeMode::System;
    m_checkUpdates = true;
    const QSettings storage(AppSettings::storagePath(), yamlFormat());
    qCDebug(settingsLog) << "loading settings from" << storage.fileName();
    // Preferred `ui/` keys, with the pre-nesting bare keys as
    // fallback (files written before nesting).
    bool ok = false;
    const double scale =
        storage.value(QStringLiteral("ui/scale"), storage.value(QStringLiteral("scale"), 1.0))
            .toDouble(&ok);
    if (ok) {
        m_scale = qBound(0.5, scale, 3.0);
    }
    m_style = storage.value(QStringLiteral("ui/style"), storage.value(QStringLiteral("style")))
                  .toString();
    const int reading =
        storage
            .value(QStringLiteral("ui/reading"),
                   storage.value(QStringLiteral("reading"), static_cast<int>(ReadingDisplay::Kana)))
            .toInt(&ok);
    if (ok && reading >= static_cast<int>(ReadingDisplay::Kana) &&
        reading <= static_cast<int>(ReadingDisplay::Kunrei)) {
        m_reading = static_cast<ReadingDisplay>(reading);
    }
    const int mode =
        storage
            .value(QStringLiteral("ui/mode"),
                   storage.value(QStringLiteral("mode"), static_cast<int>(ThemeMode::System)))
            .toInt(&ok);
    if (ok && mode >= static_cast<int>(ThemeMode::System) &&
        mode <= static_cast<int>(ThemeMode::Light)) {
        m_mode = static_cast<ThemeMode>(mode);
    }
    m_checkUpdates = storage.value(QStringLiteral("ui/checkUpdates"), m_checkUpdates).toBool();
}

void AppSettings::apply() const {
    // Style first: creating it can reset the application font on
    // some styles, so the scaled font below always wins last.
    // Palette with it (a forced mode owns the app colors).
    applyStyle();
    applyPalette();
    // Fresh desktop base on every apply (never a stale static, never
    // compounded): scale stays absolute against the system font even
    // if the desktop font changes mid-session.
    const QFont base = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    const QFont scaled = scaledFont(base, m_scale);
    QApplication::setFont(scaled);
}

QFont AppSettings::scaledFont(const QFont &base, double scale) {
    // Preserve the sizing mode: pointSizeF() is -1 on pixel-sized
    // fonts, where scaling points is a silent no-op (the font would
    // never apply and every text would stay at 100%).
    QFont out = base;
    if (base.pointSizeF() > 0.0) {
        out.setPointSizeF(base.pointSizeF() * scale);
    } else if (base.pixelSize() > 0) {
        out.setPixelSize(qRound(base.pixelSize() * scale));
    }
    return out;
}

void AppSettings::applyStyle() const {
    // Empty means the OS default: recreate it explicitly (leaving
    // the previous object installed would keep the last theme).
    QString name = m_style.isEmpty() ? osStyleName() : m_style;
    if (m_mode != ThemeMode::System &&
        (name.startsWith(QStringLiteral("kvantum"), Qt::CaseInsensitive) ||
         name.startsWith(QStringLiteral("windows11"), Qt::CaseInsensitive))) {
        // These styles paint from their own theme source and ignore
        // the forced palette (Kvantum its theme config, windows11
        // the Windows system theme — its controls go blind under a
        // forced palette), so Dark/Light would not hold — fall back
        // to a palette-driven style (Breeze first: native on KDE).
        const QStringList installed = QStyleFactory::keys();
        for (const QString &candidate : {QStringLiteral("breeze"), QStringLiteral("fusion")}) {
            const auto match =
                std::find_if(installed.begin(), installed.end(), [&](const QString &key) {
                    return key.compare(candidate, Qt::CaseInsensitive) == 0;
                });
            if (match != installed.end()) {
                name = *match;
                break;
            }
        }
    }
    if (QStyle *style = QStyleFactory::create(name)) {
        QApplication::setStyle(style);
    }
}

void AppSettings::applyPalette() const {
    // Forced modes set the application palette outright (style
    // repolish + PaletteChange refreshes repaint everything live);
    // System restores the selected style's own palette — the
    // startup OS one only when no style is chosen (a cross-style
    // restore, e.g. vista style under a windows11-captured white,
    // repaints natively-painted surfaces the wrong color).
    switch (m_mode) {
    case ThemeMode::Dark:
        QApplication::setPalette(darkForcedPalette());
        break;
    case ThemeMode::Light:
        QApplication::setPalette(lightForcedPalette());
        break;
    case ThemeMode::System:
        QApplication::setPalette(systemPalette());
        break;
    }
}

/// Palette for System mode: the explicitly chosen style's own
/// standard palette, else the startup OS capture.
QPalette AppSettings::systemPalette() const {
    if (!m_style.isEmpty()) {
        if (QStyle *probed = QStyleFactory::create(m_style)) {
            QPalette palette = probed->standardPalette();
            delete probed;
            return palette;
        }
    }
    return osPaletteSlot();
}

bool AppSettings::darkPaper() const {
    // A forced mode wins outright (the style's own palette may
    // disagree — e.g. Fusion adapts its standard palette to a
    // dark desktop even under a forced Light).
    switch (m_mode) {
    case ThemeMode::Dark:
        return true;
    case ThemeMode::Light:
        return false;
    case ThemeMode::System:
        break;
    }
    QPalette palette = QApplication::palette();
    if (!m_style.isEmpty()) {
        if (QStyle *probed = QStyleFactory::create(m_style)) {
            palette = probed->standardPalette();
            delete probed;
        }
    }
    return MaterialPalette::isDarkPalette(palette);
}

QString AppSettings::storagePath() {
    if (qEnvironmentVariableIsSet("BAKAJITEN_SETTINGS")) {
        return qEnvironmentVariable("BAKAJITEN_SETTINGS");
    }
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
           QStringLiteral("/settings.yaml");
}

QString AppSettings::memorizedPath() {
    return QFileInfo(storagePath()).absolutePath() + QStringLiteral("/state.yaml");
}

void AppSettings::resetStorage() {
    QSettings storage(AppSettings::storagePath(), yamlFormat());
    storage.clear();
    storage.sync();
    // First write also needs the directory (explicit filename).
    QDir().mkpath(QFileInfo(storagePath()).absolutePath());
}

void AppSettings::noteOsStyle() { osStyleSlot() = QApplication::style()->objectName(); }

void AppSettings::noteOsPalette() { (void)osPaletteSlot(); }

QString AppSettings::osStyleName() {
    const QString &captured = osStyleSlot();
    return captured.isEmpty() ? QApplication::style()->objectName() : captured;
}
