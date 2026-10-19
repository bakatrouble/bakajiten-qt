#include "core/dictionary/DictionaryModel.h"

#include "core/dictionary/DictStore.h"
#include "dict.hpp"
#include "util/Romaji.h"

#include <QDebug>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

const DictionaryModel::CachedRow &DictionaryModel::rowAt(int row) const {
    std::optional<CachedRow> &slot = cache[static_cast<std::size_t>(row)];
    if (!slot.has_value()) {
        CachedRow fresh;
        const dict::SearchHit &hit = rows[static_cast<std::size_t>(row)];
        try {
            if (const std::optional<dict::DictEntry> entry = DictStore::shared().entryAt(hit.idx)) {
                QStringList kebs;
                for (const dict::DictKanjiElement &k : entry->kanji) {
                    kebs << QString::fromStdString(k.keb);
                }
                QStringList rebs;
                for (const dict::DictReadingElement &r : entry->readings) {
                    rebs << QString::fromStdString(r.reb);
                }
                fresh.heading = matchForm(kebs, lastQuery, QString::fromStdString(entry->heading));
                fresh.reading = matchForm(rebs, lastQuery, QString::fromStdString(entry->reading));
                fresh.meaning = formatSenses(*entry);
                fresh.senses = displaySenses(*entry);
                fresh.headingIsKana = Romaji::isKana(fresh.heading);
                fresh.entSeq = entry->ent_seq;
            }
        } catch (...) {
            // FFI failure means corrupt data: log once and leave the
            // row empty rather than unwinding through paint.
            qWarning() << "DictionaryModel: entry_at failed for row" << row;
        }
        slot = std::move(fresh);
    }
    return *slot;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
QString DictionaryModel::matchForm(const QStringList &forms, const QString &query,
                                   const QString &fallback) {
    if (!query.isEmpty()) {
        for (const QString &form : forms) {
            if (form.contains(query)) {
                return form;
            }
        }
    }
    return fallback;
}

QString DictionaryModel::formatSenses(const dict::DictEntry &entry) {
    QStringList senses;
    for (const dict::DictSense &sense : entry.senses) {
        QStringList glosses;
        for (const std::string &gloss : sense.glosses) {
            glosses << QString::fromStdString(gloss);
        }
        // Senses without English glosses (foreign-only) contribute
        // nothing — no empty `|` segments.
        if (!glosses.isEmpty()) {
            senses << glosses.join(QStringLiteral("; "));
        }
    }
    return senses.join(QStringLiteral(" | "));
}

/// One abbreviated label (`&n;` → `n`; anything else passes through;
//  invalid indices resolve empty and are skipped).
namespace {
QString abbreviateTag(const std::string &label) {
    QString abbreviated = QString::fromStdString(label);
    if (abbreviated.startsWith(QLatin1Char('&')) && abbreviated.endsWith(QLatin1Char(';'))) {
        abbreviated = abbreviated.mid(1, abbreviated.size() - 2);
    }
    return abbreviated;
}
} // namespace

/// Every sense tag, abbreviated and deduplicated: the delegate paints
/// the joined labels small and green ahead of the meaning body.
QVector<SenseDisplay> DictionaryModel::displaySenses(const dict::DictEntry &entry) {
    QVector<SenseDisplay> out;
    for (const dict::DictSense &sense : entry.senses) {
        QStringList glosses;
        for (const std::string &gloss : sense.glosses) {
            glosses << QString::fromStdString(gloss);
        }
        if (glosses.isEmpty()) {
            continue; // foreign-only sense: no runs at all
        }
        SenseDisplay display;
        display.gloss = glosses.join(QStringLiteral("; "));
        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        auto abbreviateAll = [&](const auto &indices, auto labelOf, auto tipOf, QStringList &out,
                                 QStringList &tipOut) {
            for (const std::uint8_t tag : indices) {
                const QString abbreviated = abbreviateTag(labelOf(tag));
                if (!abbreviated.isEmpty() && !out.contains(abbreviated)) {
                    out << abbreviated;
                    tipOut << QString::fromStdString(tipOf(tag));
                }
            }
        };
        abbreviateAll(
            sense.pos,
            [&](std::uint8_t tag) {
                return entry.is_name ? dict::name_type_label(tag) : dict::pos_label(tag);
            },
            [&](std::uint8_t tag) {
                return entry.is_name ? dict::name_type_description(tag)
                                     : dict::pos_description(tag);
            },
            display.pos, display.posTips);
        abbreviateAll(sense.field, dict::field_label, dict::field_description, display.field,
                      display.fieldTips);
        abbreviateAll(sense.misc, dict::misc_label, dict::misc_description, display.misc,
                      display.miscTips);
        for (const std::string &s : sense.xref) {
            display.xref << QString::fromStdString(s);
        }
        for (const std::string &s : sense.ant) {
            display.ant << QString::fromStdString(s);
        }
        for (const std::string &s : sense.stagk) {
            display.onlyKeb << QString::fromStdString(s);
        }
        for (const std::string &s : sense.stagr) {
            display.onlyReb << QString::fromStdString(s);
        }
        for (const std::string &s : sense.info) {
            display.notes << QString::fromStdString(s);
        }
        for (const dict::LangSource &l : sense.sources) {
            SourceDisplay source;
            if (l.lang.has_value()) {
                source.lang = QString::fromStdString(l.lang.value());
            }
            source.text = QString::fromStdString(l.text);
            display.sources << source;
        }
        out << display;
    }
    return out;
}

ArticleDisplay DictionaryModel::articleFor(int row) const {
    ArticleDisplay article;
    if (!ready || row < 0 || row >= static_cast<int>(rows.size())) {
        return article;
    }
    try {
        const dict::SearchHit &hit = rows[static_cast<std::size_t>(row)];
        const std::optional<dict::DictEntry> entry = DictStore::shared().entryAt(hit.idx);
        if (!entry) {
            return article;
        }
        article.heading = QString::fromStdString(entry->heading);
        article.reading = QString::fromStdString(entry->reading);
        article.entSeq = entry->ent_seq;
        article.jlpt = entry->jlpt;
        for (const dict::DictKanjiElement &k : entry->kanji) {
            ElementDisplay element;
            element.form = QString::fromStdString(k.keb);
            for (const std::uint8_t tag : k.info) {
                const QString abbreviated = abbreviateTag(dict::inf_label(tag));
                if (!abbreviated.isEmpty() && !element.info.contains(abbreviated)) {
                    element.info << abbreviated;
                }
            }
            for (const std::string &code : k.priorities) {
                element.priorities << QString::fromStdString(code);
            }
            article.kanji << element;
        }
        for (const dict::DictReadingElement &r : entry->readings) {
            ElementDisplay element;
            element.form = QString::fromStdString(r.reb);
            element.noKanji = r.nokanji;
            for (const std::string &keb : r.restricted_to) {
                element.restrictedTo << QString::fromStdString(keb);
            }
            for (const std::uint8_t tag : r.info) {
                const QString abbreviated = abbreviateTag(dict::inf_label(tag));
                if (!abbreviated.isEmpty() && !element.info.contains(abbreviated)) {
                    element.info << abbreviated;
                }
            }
            for (const std::string &code : r.priorities) {
                element.priorities << QString::fromStdString(code);
            }
            article.readings << element;
        }
        article.senses = displaySenses(*entry);
    } catch (...) {
        // Same policy as row materialization: corrupt data leaves
        // an empty article, never unwinds.
        qWarning() << "DictionaryModel: articleFor failed for row" << row;
    }
    return article;
}

QVector<dict::DictEntry> DictionaryModel::wordsContaining(const QString &ch, uint cap) const {
    QVector<dict::DictEntry> out;
    if (!ready || cap == 0) {
        return out;
    }
    try {
        for (const dict::DictEntry &entry :
             DictStore::shared().searchMiddle(ch.toStdString(), static_cast<uint32_t>(cap))) {
            out << entry;
        }
    } catch (...) {
        // Same policy as row materialization: corrupt data yields
        // nothing, never unwinds.
        qWarning() << "DictionaryModel: wordsContaining failed";
    }
    return out;
}
