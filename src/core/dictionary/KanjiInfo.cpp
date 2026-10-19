#include "core/dictionary/KanjiInfo.h"
#include "core/dictionary/DictStore.h"

#include <QPointF>
#include <QStringList>

KanjiInfo::KanjiInfo(QObject *parent) : QObject(parent) {}
bool KanjiInfo::load(const QString &kanjiPath) { return DictStore::shared().loadKanji(kanjiPath); }

/// Up to this many readings/meanings per card (tooltips stay short).
namespace {
constexpr int maxReadings = 3;
constexpr int maxMeanings = 3;
} // namespace

bool KanjiInfo::isKanjiChar(const QString &ch) {
    const QList<uint> codepoints = ch.toUcs4();
    if (codepoints.size() != 1) {
        return false;
    }
    const uint c = codepoints.front();
    if (c < 0x4E00 || c > 0x9FFF) {
        if (c < 0x3400 || c > 0x4DBF) {
            if (c < 0xF900 || c > 0xFAFF) {
                if (c < 0x20000 || c > 0x2FFFF) {
                    return false;
                }
            }
        }
    }
    return true;
}

QString KanjiInfo::radicalGlyph(uint32_t number) {
    // Glyph table lives in Rust (same traces); Qt only converts.
    return QString::fromStdString(dict::radical_glyph(number));
}

QStringList KanjiInfo::infoLines(const QString &ch) {
    if (!isKanjiChar(ch) || !ready()) {
        return {};
    }
    if (ch == lastChar) {
        return lastLines;
    }
    lastChar = ch;
    lastLines.clear();
    const std::optional<dict::KanjiEntry> entry = DictStore::shared().lookupKanji(ch.toStdString());
    if (!entry) {
        return {};
    }
    const auto capped = [](const std::vector<std::string> &items) {
        QStringList out;
        for (const std::string &item : items) {
            if (out.size() >= maxReadings) {
                break;
            }
            out << QString::fromStdString(item);
        }
        return out;
    };
    const QStringList readings = capped(entry->on) + capped(entry->kun);
    if (!readings.isEmpty()) {
        lastLines << readings.join(QStringLiteral("、"));
    }
    QStringList meanings;
    for (const std::string &meaning : entry->meanings) {
        if (meanings.size() >= maxMeanings) {
            break;
        }
        meanings << QString::fromStdString(meaning);
    }
    if (!meanings.isEmpty()) {
        QString joined = meanings.join(QStringLiteral("; "));
        if (entry->meanings.size() > static_cast<std::size_t>(maxMeanings)) {
            joined += QStringLiteral(" …");
        }
        lastLines << joined;
    }
    lastLines << tr("%1 strokes").arg(entry->strokes);
    return lastLines;
}

KanjiInfo::Display KanjiInfo::display(const QString &ch) {
    Display out;
    if (!isKanjiChar(ch) || !ready()) {
        return out;
    }
    const std::optional<dict::KanjiEntry> entry = DictStore::shared().lookupKanji(ch.toStdString());
    if (!entry) {
        return out;
    }
    out.character = ch;
    out.strokes = entry->strokes;
    out.radical = entry->radical.value_or(0);
    constexpr int maxCardReadings = 3;
    constexpr int maxCardMeanings = 4;
    for (const std::string &r : entry->on) {
        if (out.on.size() >= maxCardReadings) {
            break;
        }
        out.on << QString::fromStdString(r);
    }
    for (const std::string &r : entry->kun) {
        if (out.kun.size() >= maxCardReadings) {
            break;
        }
        out.kun << QString::fromStdString(r);
    }
    for (const std::string &m : entry->meanings) {
        if (out.meanings.size() >= maxCardMeanings) {
            break;
        }
        out.meanings << QString::fromStdString(m);
    }
    out.grade = entry->grade.value_or(0);
    out.jlpt = entry->jlpt.value_or(0);
    out.freq = entry->freq.value_or(0);
    for (const std::string &p : entry->parts) {
        out.parts << QString::fromStdString(p);
    }
    if (entry->jis208.has_value()) {
        out.jis = jisHex(entry->jis208->ku, entry->jis208->ten);
    }
    // Full kanjidic detail for the dialog (uncapped — the dialog
    // scrolls; the tooltip caps above).
    for (const std::string &r : entry->nanori) {
        out.nanori << QString::fromStdString(r);
    }
    for (const std::string &r : entry->rad_names) {
        out.radNames << QString::fromStdString(r);
    }
    for (const std::string &r : entry->chinese) {
        // Marked tones (shì, not shi4): the pack stores numbered
        // pinyin, display marks it.
        out.chinese << QString::fromStdString(dict::pinyin_marked(r));
    }
    for (const std::string &r : entry->korean) {
        out.korean << QString::fromStdString(r);
    }
    for (const std::string &r : entry->vietnamese) {
        out.vietnamese << QString::fromStdString(r);
    }
    for (const std::string &c : entry->skip) {
        out.skip << QString::fromStdString(c);
    }
    out.skipMisgrade = entry->skip_misgrade;
    for (const std::string &c : entry->four_corner) {
        out.fourCorner << QString::fromStdString(c);
    }
    for (const std::string &c : entry->deroo) {
        out.deroo << QString::fromStdString(c);
    }
    for (const std::string &c : entry->sh_desc) {
        out.shDesc << QString::fromStdString(c);
    }
    for (const std::string &c : entry->jis208_refs) {
        out.jis208Refs << QString::fromStdString(c);
    }
    for (const std::string &c : entry->jis212) {
        out.jis212 << QString::fromStdString(c);
    }
    for (const std::string &c : entry->jis213) {
        out.jis213 << QString::fromStdString(c);
    }
    for (const std::string &c : entry->deroo_variant) {
        out.derooVariant << QString::fromStdString(c);
    }
    for (const std::string &c : entry->nelson_c) {
        out.nelsonC << QString::fromStdString(c);
    }
    for (const std::string &c : entry->njecd) {
        out.njecd << QString::fromStdString(c);
    }
    for (const std::string &c : entry->oneill) {
        out.oneill << QString::fromStdString(c);
    }
    for (const std::string &c : entry->s_h) {
        out.shRefs << QString::fromStdString(c);
    }
    out.valid = true;
    return out;
}

QString KanjiInfo::jisHex(uint8_t ku, uint8_t ten) {
    return QString::fromStdString(dict::jis_hex(ku, ten));
}

QString KanjiInfo::gradeName(uint32_t grade) {
    // Category in Rust, translations here.
    switch (dict::grade_kind(grade)) {
    case dict::GradeKind::kKyouiku:
        return tr("Kyouiku");
    case dict::GradeKind::kJouyou:
        return tr("Jouyou");
    case dict::GradeKind::kJinmeiyou:
        return tr("Jinmeiyou");
    default:
        return tr("Grade");
    }
}

QStringList KanjiInfo::containing(const QString &part) {
    QStringList out;
    if (!ready()) {
        return out;
    }
    // isKanjiChar gates single Han code points (parts are kanji or
    // radicals, never kana).
    if (!isKanjiChar(part)) {
        return out;
    }
    for (const std::string &c : DictStore::shared().kanjiContaining(part.toStdString())) {
        out << QString::fromStdString(c);
    }
    return out;
}

QVector<QVector<QPointF>> KanjiInfo::strokes(const QString &ch) {
    QVector<QVector<QPointF>> out;
    if (!isKanjiChar(ch) || !ready()) {
        return out;
    }
    for (const dict::KanjiStroke &stroke : DictStore::shared().kanjiStrokes(ch.toStdString())) {
        QVector<QPointF> points;
        for (const dict::KanjiPoint &p : stroke.points) {
            points << QPointF(p.x, p.y);
        }
        out << points;
    }
    return out;
}
