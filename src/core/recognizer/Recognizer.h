#pragma once

#include <QPointF>
#include <QString>
#include <QVector>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace recognizer {
struct Recognizer;
}

/// Handwriting recognizer: thin Qt-flavored wrapper over the UniFFI C++
/// bindings (`recognizer.hpp`, generated at build time from
/// `librecognizer/src/ffi.rs` — see CMakeLists). Sources load from
/// the converted bundle (`network.safetensors`, converted once
/// from Google's packs by the Dictionaries dialog).
/// Global singleton — one loaded Japanese pack for the panel
/// matcher alike (null = no model); strokes stay with the caller
/// (the widget already keeps them for painting) and are passed per
/// query. Thread-safe (one mutex around the handle); plain C++, no
/// signals.
///
/// The pack downloads from `dl.google.com` and converts under the
/// model dir on first use (see `ensureModel`); later calls reuse the
/// cached handle. Blocking: only call it on the match thread.
class Recognizer {
public:
    /// One decoded hypothesis: display text plus score.
    struct Hit {
        QString text;
        double score = 0.0;
    };
    /// Global handle (leaked at exit — the app always needs it).
    static Recognizer &shared();

    /// Load the cached Japanese pack (no download); false when
    /// missing or rejected (handle cleared either way).
    bool loadModel();
    /// Drop the loaded handle (the next query reloads from disk).
    void reset();
    bool hasModel() const;
    /// The loaded pack carries the word language model.
    bool hasLanguageModel() const;

    /// Decode finished pen strokes (best first, at most `topK`);
    /// empty when there is no model or no strokes.
    /// Thread-safe against concurrent decodes (the handle is copied
    /// under the lock; scoring runs lock-free).
    std::vector<Hit> recognize(const QVector<QVector<QPointF>> &strokes, std::uint32_t topK) const;

private:
    Recognizer() = default;

    /// Read the cached pack into the handle (no download).
    /// Call with the mutex held.
    bool loadLocked();

    mutable std::mutex mutex;
    std::shared_ptr<recognizer::Recognizer> recognizer;
};
