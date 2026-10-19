#pragma once

#include <QString>

#include <cstdint>

#include "core/settings/Settings.h"

/// Kana → romaji for reading display (the query edge converts the
/// other way). Hepburn uses macrons,
/// Kunrei-shiki circumflexes. Long vowels merge (`aa` → ā, `ou` →
/// ō) except word-final `ou` (verb endings like かう stay split —
/// spelling cannot tell them from nouns like きょう, which renders
/// "kyou"); Hepburn additionally keeps `ei` split (せんせい, not
/// sensē). Both fold katakana first and pass non-kana through
/// untouched. を never merges.
enum class RomajiStyle : std::uint8_t { Hepburn, Kunrei };

namespace Romaji {
/// Convert a kana string (hiragana or katakana) to romaji.
QString toRomaji(const QString &kana, RomajiStyle style);
/// True when the string holds no kanji (a reading in effect —
/// kana-only headings romanize with the readings).
bool isKana(const QString &s);
/// Reading (or kana-only heading) under the live setting.
QString displayReading(const QString &raw);
} // namespace Romaji
