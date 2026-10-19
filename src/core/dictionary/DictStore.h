#pragma once

#include "core/recognizer/Recognizer.h"
#include "dict.hpp"

#include <QString>

#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

/// Global owner of every libdict handle (words/names dictionary +
/// kanji dictionary) and every stateful libdict operation. Plain
/// C++, thread-safe: loads take the write lock, queries run
/// concurrently under shared locks. Stateless converters (romaji,
/// pinyin, tag labels) stay direct `dict::` calls — no state to
/// own, nothing to gain. All methods are empty/false-safe on
/// missing handles (loads happen once at startup; callers keep
/// their own readiness guards too).
class DictStore {
public:
    /// Global store (leaked at exit — the app always needs it).
    static DictStore &shared();

    /// Map one dictionary artifact (false when missing or
    /// rejected — the handle is cleared either way, so reloads
    /// never strand a stale file).
    bool loadDictionary(const QString &path);
    /// Map the kanji artifact (same contract).
    bool loadKanji(const QString &kanjiPath);
    bool dictionaryReady() const;
    bool kanjiReady() const;
    std::uint64_t entryCount() const;
    std::uint64_t kanjiCount() const;

    /// Tiered index lookup over the loaded file, `cap` total.
    std::vector<dict::SearchHit> searchIndices(const std::string &query, dict::SearchMode mode,
                                               std::uint32_t cap);
    /// English gloss substring lookup over the loaded file — the
    /// meaning-search mode. No deinflection, no edge modes; hits
    /// carry empty chains (never tinted).
    std::vector<dict::SearchHit> searchMeaningIndices(const std::string &query, std::uint32_t cap);
    /// One decoded entry frame (rows materialize through this).
    std::optional<dict::DictEntry> entryAt(std::uint32_t idx);
    /// Word entries for one kanji, relevance-ranked (dialog Words
    /// tab).
    std::vector<dict::DictEntry> searchMiddle(const std::string &character, std::uint32_t cap);
    /// Structured kanji metadata (single-char string; empty option
    /// when absent).
    std::optional<dict::KanjiEntry> lookupKanji(const std::string &character);
    /// KanjiVG stroke polylines in pen space, in stroke order.
    std::vector<dict::KanjiStroke> kanjiStrokes(const std::string &character);
    /// Characters using `part` (kradfile order).
    std::vector<std::string> kanjiContaining(const std::string &part);
    /// Shape-similar kanji for one character: strokes from the
    /// kanji artifact, matched through the shared recognizer bank
    /// (panel matcher and similar strip share it), the character
    /// itself dropped, at most `topK` shown.
    std::vector<Recognizer::Hit> similarKanji(const QString &ch, std::uint32_t topK);

private:
    DictStore() = default;

    mutable std::shared_mutex mutex;
    std::shared_ptr<dict::Dictionary> dictionary;
    std::shared_ptr<dict::KanjiDictionary> kanji;
};
