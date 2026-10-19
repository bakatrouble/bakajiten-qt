#pragma once

#include <QString>
#include <QVector>

#include <optional>

/// Furigana segmentation: split a headword form against its reading
/// into kana-literal runs and kanji ruby groups (`食べる`/`たべる` →
/// `食`/`た` + literal `べる`; `今日`/`きょう` → one group). Greedy
/// outside-in with backtracking (repeated-kana ambiguity like
/// `窺う`/`うかがう` needs more than prefix/suffix stripping) and
/// katakana↔hiragana folding (`ガリ勉`/`がりべん`). Pure raw
/// strings — display transliteration (Romaji setting) applies to the
/// pieces at render time, never here.
class Furigana {
public:
    struct Span {
        /// Literal kana run (empty ruby) or ruby group.
        QString text;
        QString ruby;
        bool hasRuby() const { return !ruby.isEmpty(); }
    };

    /// Segments, or nullopt when the pair does not align (ateji,
    /// phrases) — and when the head needs no ruby at all
    /// (kana-only): both render as today's plain heading.
    /// (Implemented in Rust — `furigana_segment` — same traces.)
    static std::optional<QVector<Span>> segment(const QString &head, const QString &reading);
};
