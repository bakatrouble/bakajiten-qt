#include "util/Romaji.h"

#include "dict.hpp"

#include <QString>

QString Romaji::toRomaji(const QString &kana, RomajiStyle style) {
    // Conversion tables and loop live in Rust (same traces);
    // the style enum stays trackside with the setting.
    return QString::fromStdString(dict::kana_to_romaji(
        kana.toStdString(),
        style == RomajiStyle::Hepburn ? dict::RomajiStyle::kHepburn : dict::RomajiStyle::kKunrei));
}
