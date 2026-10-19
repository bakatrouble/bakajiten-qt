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

Q_LOGGING_CATEGORY(settingsLog, "ui.settings");

namespace {
/// OS-default style captured at startup (before overrides).
QString &osStyleSlot() {
    static QString name;
    return name;
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
           m_icons == other.m_icons && m_reading == other.m_reading &&
           m_checkUpdates == other.m_checkUpdates;
}

void AppSettings::save() const {
    QSettings storage(AppSettings::storagePath(), yamlFormat());
    storage.setValue(QStringLiteral("ui/scale"), m_scale);
    storage.setValue(QStringLiteral("ui/style"), m_style);
    storage.setValue(QStringLiteral("ui/icons"), static_cast<int>(m_icons));
    storage.setValue(QStringLiteral("ui/reading"), static_cast<int>(m_reading));
    storage.setValue(QStringLiteral("ui/checkUpdates"), m_checkUpdates);
    storage.sync();
}

void AppSettings::load() {
    // Defaults first; missing keys keep them.
    m_scale = 1.0;
    m_style.clear();
    m_icons = IconScheme::Auto;
    m_reading = ReadingDisplay::Kana;
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
    const int icons =
        storage
            .value(QStringLiteral("ui/icons"),
                   storage.value(QStringLiteral("icons"), static_cast<int>(IconScheme::Auto)))
            .toInt(&ok);
    if (ok && icons >= static_cast<int>(IconScheme::Auto) &&
        icons <= static_cast<int>(IconScheme::Dark)) {
        m_icons = static_cast<IconScheme>(icons);
    }
    const int reading =
        storage
            .value(QStringLiteral("ui/reading"),
                   storage.value(QStringLiteral("reading"), static_cast<int>(ReadingDisplay::Kana)))
            .toInt(&ok);
    if (ok && reading >= static_cast<int>(ReadingDisplay::Kana) &&
        reading <= static_cast<int>(ReadingDisplay::Kunrei)) {
        m_reading = static_cast<ReadingDisplay>(reading);
    }
    m_checkUpdates = storage.value(QStringLiteral("ui/checkUpdates"), m_checkUpdates).toBool();
}

void AppSettings::apply() const {
    // Style first: creating it can reset the application font on
    // some styles, so the scaled font below always wins last.
    applyStyle();
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
    const QString name = m_style.isEmpty() ? osStyleName() : m_style;
    if (QStyle *style = QStyleFactory::create(name)) {
        QApplication::setStyle(style);
    }
}

IconScheme AppSettings::resolvedIcons() const {
    if (m_icons != IconScheme::Auto) {
        return m_icons;
    }
    QPalette palette = QApplication::palette();
    if (!m_style.isEmpty()) {
        if (QStyle *probed = QStyleFactory::create(m_style)) {
            palette = probed->standardPalette();
            delete probed;
        }
    }
    return MaterialPalette::isDarkPalette(palette) ? IconScheme::Light : IconScheme::Dark;
}

AppSettings &AppSettings::shared() {
    static AppSettings settings = AppSettings::fromStorage();
    return settings;
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

QString AppSettings::osStyleName() {
    const QString &captured = osStyleSlot();
    return captured.isEmpty() ? QApplication::style()->objectName() : captured;
}
