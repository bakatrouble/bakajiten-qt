#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

/// One manifest file entry: packed size, gzipped size, provenance
/// version, and the content hash (the only change signal — the
/// version strings move on days the bytes don't).
struct DataFileEntry {
    qint64 size = 0;
    qint64 gz = 0;
    QString version;
    QString sha256;
    QStringList sources;
};

/// Parsed `manifest.json`: release build date plus one entry per
/// artifact (see `data-repo/pack.py` for the writer).
struct DataManifest {
    QString buildDate;
    QMap<QString, DataFileEntry> files;
};

/// Local file state against a manifest entry (missing files and
/// size mismatches never hash — only a size match needs the
/// sha256 read to confirm it is current).
enum class DataFileStatus : std::uint8_t { Missing, Outdated, Current };

/// Manifest parsing plus local-file comparison. Pure logic, no
/// network: unit-tested without fixtures.
class DataManifestReader {
public:
    static std::optional<DataManifest> parse(const QByteArray &bytes);
    static std::optional<DataManifest> readFile(const QString &path);
    /// Hex sha256 of a file (empty when unreadable).
    static QString sha256Of(const QString &path);
    /// Hex sha256 of bytes in memory.
    static QString sha256Of(const QByteArray &bytes);
    static DataFileStatus status(const DataFileEntry &entry, const QString &localPath);
};
