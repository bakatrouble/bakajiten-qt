#pragma once

#include <QString>

#include <cstdint>
#include <vector>

/// Google's Japanese handwriting pack set (dl.google.com zips).
/// Catalog facts (pack names, URLs, sha1 checksums, sizes) come
/// from the `digital-ink-recognition` AAR's `assets/manifest.json`
/// (19.0.0); the zips are plain unauthenticated downloads.
/// Layout under the model root:
///
/// ```text
/// <root>/ja/recospec             the recognizer spec (protobuf, converts into the bundle)
/// <root>/ja/network.tflite       the script network (converts into the bundle)
/// <root>/ja/network.safetensors  the converted bundle (net + vocab metadata: what loads)
/// <root>/.zips/<pack>.zip        cached pack zips (sha1-verified)
/// ```
///
/// The word-level language model pack (`lm.fst`) is no longer
/// fetched: the engine decodes from the net plus the
/// character-class table alone (the compact FST is a proprietary
/// format), and single-kanji lookup ranks identically without it.
/// Fixed role file names (not the zips' member names) so a cache
/// hit is a pure existence + size check, with no unzip involved.
/// Single source of truth for the Qt side: the recognizer loads
/// from these paths, the Dictionaries dialog downloads them.
class ModelPacks {
public:
    /// One Google pack zip: everything needed to fetch and verify it.
    struct Pack {
        const char *kind;         // recospec | tflite
        const char *pack;         // pack name in Google's catalog
        const char *url;          // dl.google.com download URL
        const char *sha1Hex;      // lowercase sha1 of the zip
        qint64 zipSize;           // zip bytes (download total)
        const char *memberSuffix; // the wanted member ends with this
        const char *roleFile;     // installed file name under ja/
        qint64 artifactSize;      // extracted member bytes
    };

    /// Model root: `<data>/mlkit-ink`, with an explicit `MODEL`
    /// override for dev runs.
    static QString rootDir();
    /// Language tag the apps ship.
    static QString tag();
    /// The `ja` pack set, in load order.
    static std::vector<Pack> packs();
    /// Installed artifact path for one pack.
    static QString artifactPath(const Pack &pack);
    /// Installed artifact path for one pack under an explicit root.
    static QString artifactPath(const QString &rootDir, const Pack &pack);
    /// Cached pack-zip path for one pack.
    static QString zipPath(const Pack &pack);
    /// Cached pack-zip path for one pack under an explicit root.
    static QString zipPath(const QString &rootDir, const Pack &pack);
    /// Converted bundle path under an explicit root (what loads).
    static QString safetensorsPath(const QString &rootDir);
    /// Converted bundle path (what loads).
    static QString safetensorsPath();
    /// Byte length of the converted bundle (single source: the
    /// `EXPECTED_SAFETENSORS_LEN` the recognizer tests pin).
    static qint64 safetensorsSize();
    /// Sum of the pack zips (the honest download total).
    static qint64 downloadBytes();
    /// True when the converted bundle is present at its catalog
    /// size (sources may come and go; only the bundle loads).
    static bool isCached();

private:
    ModelPacks() = delete;
};
