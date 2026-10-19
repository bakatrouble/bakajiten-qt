#pragma once

#include <QString>

/// Remote home of the packed dictionary artifacts: the
/// `bakajiten-data` repo's releases (`data-YYYYMMDD` tags, daily
/// when upstream moves). The app downloads the `.gz` copies and
/// unpacks them with the system zlib (the static Qt builds use
/// `-system-zlib`, so it is the same copy Qt itself uses).
namespace DataSource {
inline QString releaseBase() {
    return QStringLiteral("https://github.com/bakatrouble/bakajiten-data/releases/latest/download");
}
inline QString manifestUrl() { return releaseBase() + QStringLiteral("/manifest.json"); }
inline QString fileUrl(const QString &file) {
    return releaseBase() + QLatin1Char('/') + file + QStringLiteral(".gz");
}
} // namespace DataSource
