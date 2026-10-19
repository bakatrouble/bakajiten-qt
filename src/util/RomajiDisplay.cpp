#include "util/Romaji.h"

#include "core/settings/Settings.h"
#include "dict.hpp"

bool Romaji::isKana(const QString &s) { return dict::is_kana(s.toStdString()); }

QString Romaji::displayReading(const QString &raw) {
    switch (AppSettings::shared().reading()) {
    case ReadingDisplay::Hepburn:
        return toRomaji(raw, RomajiStyle::Hepburn);
    case ReadingDisplay::Kunrei:
        return toRomaji(raw, RomajiStyle::Kunrei);
    case ReadingDisplay::Kana:
        return raw;
    }
    return raw;
}
