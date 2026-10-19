#pragma once

#include <QPointF>
#include <QString>
#include <QVector>

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace recognizer {
struct Recognizer;
}

/// Handwriting recognizer: thin Qt-flavored wrapper over the UniFFI C++
/// bindings (`recognizer.hpp`, generated at build time from
/// `librecognizer/src/ffi.rs` — see CMakeLists). Global singleton —
/// one template bank for the panel matcher and the kanji dialog's
/// similar strip alike (null = no model); strokes stay with the
/// caller (the widget already keeps them for painting) and are
/// passed per query. Thread-safe (one mutex around the bank);
/// plain C++, no signals.
class Recognizer {
public:
    /// One ranked hit: display text, fused distance (lower is
    /// better), and whether the template is kana (tinted in the
    /// candidate strip).
    struct Hit {
        QString text;
        std::uint64_t score = 0;
        bool kana = false;
    };

    /// Global bank (leaked at exit — the app always needs it).
    static Recognizer &shared();

    /// Load model bytes from `path`; false when unreadable or
    /// rejected (bank cleared either way).
    bool loadModel(const QString &path);
    /// Template count (0 = no model).
    std::uint64_t templateCount() const;
    bool hasModel() const;

    /// Match finished strokes in pen coords (best first, at most
    /// `topK`); empty when there is no model or no strokes.
    /// `includeKana` selects kanji+kana (true) or kanji-only.
    std::vector<Hit> recognize(const QVector<QVector<QPointF>> &strokes, std::uint32_t topK,
                               bool includeKana) const;
    /// Shape-similar kanji for stored pen-space strokes (the kanji
    /// dialog's Similar strip): kanji-only match with `exclude`
    /// dropped, capped at `topK`. Empty without strokes or a model.
    std::vector<Hit> similar(const QVector<QVector<QPointF>> &strokes, const QString &exclude,
                             std::uint32_t topK) const;

private:
    Recognizer() = default;

    mutable std::mutex mutex;
    std::shared_ptr<recognizer::Recognizer> recognizer;
};
