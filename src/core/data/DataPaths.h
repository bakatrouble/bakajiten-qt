#pragma once

#include <QString>
#include <QStringList>

/// Home of the dictionary artifacts: the directory holding
/// `settings.yaml` (created on demand). `BAKAJITEN_DATA_DIR`
/// overrides it outright (tests point it at packed repo data).
/// Exactly one dictionary loads at a time; names/kokugo are opt-in
/// downloads, the rest auto-download when missing.
class DataPaths {
public:
    /// Artifact directory (override or settings-adjacent).
    static QString directory();
    /// Create the directory (first write needs it explicitly).
    static void ensureDirectory();
    /// Full path of one artifact in the directory.
    static QString filePath(const QString &file);
    /// Full path of the downloaded manifest copy.
    static QString manifestPath();

    /// Auto-downloaded when missing (core lookup, kanji cards,
    /// handwriting bank).
    static QStringList requiredFiles();
    /// Never auto-fetched (scope combo offers them once present).
    static QStringList optionalFiles();
    static QStringList allFiles();
    static bool isOptional(const QString &file);
    /// Human label for dialogs (`English (JMdict)` et al.).
    static QString displayName(const QString &file);
};
