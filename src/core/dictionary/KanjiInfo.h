#pragma once

#include "core/dictionary/DictStore.h"

#include <QObject>
#include <QPointF>
#include <QString>
#include <QVector>

#include <optional>

/// Kanji hover info: mmapped `kanji.bin` plus one-character lookup.
/// Shared by the results table, the article pane, and the candidate
/// tooltip — one mmap per owner (shared page cache). UI-thread use:
/// lookups scan records linearly, so the last character caches.
class KanjiInfo : public QObject {
    Q_OBJECT
public:
    /// Structured card content (see the tooltip mock: glyph plus
    /// strokes/radical header, meanings, ON/Kun, grade/JLPT footer).
    /// Lists are capped for card size (readings 3, meanings 4);
    /// `radical`/`grade`/`jlpt`/`freq` are 0 when the source
    /// carries none. `jis` is the `0xXXXX` JIS X 0208 code (empty
    /// without one). The kanjidic lookup fields below ride full
    /// (dialog-only; tooltips never read them).
    struct Display {
        QString character;
        uint32_t strokes = 0;
        uint32_t radical = 0;
        QStringList on;
        QStringList kun;
        QStringList meanings;
        uint32_t grade = 0;
        uint32_t jlpt = 0;
        uint32_t freq = 0;
        QStringList parts;
        QString jis;
        QStringList nanori;
        QStringList radNames;
        QStringList chinese;
        QStringList korean;
        QStringList vietnamese;
        QStringList skip;
        bool skipMisgrade = false;
        QStringList fourCorner;
        QStringList deroo;
        QStringList shDesc;
        QStringList jis208Refs;
        QStringList jis212;
        QStringList jis213;
        QStringList derooVariant;
        QStringList nelsonC;
        QStringList njecd;
        QStringList oneill;
        QStringList shRefs;
        bool valid = false;
    };

    /// Classical Kangxi radical glyph for a 1–214 number (empty when
    /// out of range). Static table, verified against the Unicode
    /// Kangxi block names.
    static QString radicalGlyph(uint32_t number);
    /// JIS X 0208 code for kuten (`ku`, `ten` as packed): `0x385e`
    /// form. Pure display mapping, no artifact needed.
    static QString jisHex(uint8_t ku, uint8_t ten);
    /// Grade category name for the footer and dialog (`8` reads
    /// `Jouyou`, like the mocks; kyouiku/jinmeiyou otherwise).
    static QString gradeName(uint32_t grade);

    explicit KanjiInfo(QObject *parent = nullptr);

    /// Map the artifact (false when missing — tooltips stay empty).
    bool load(const QString &kanjiPath);
    bool ready() const { return DictStore::shared().kanjiReady(); }

    /// Short info lines for one character (readings, meanings,
    /// stroke count), empty when it is not a covered kanji.
    QStringList infoLines(const QString &ch);
    /// Structured card content, invalid when not covered.
    Display display(const QString &ch);
    /// KanjiVG stroke polylines for the stroke-order diagram, in
    /// stroke order (pen space 0..255). Empty when uncovered.
    QVector<QVector<QPointF>> strokes(const QString &ch);
    /// Characters whose kradfile parts contain `part` (the dialog's
    /// "part of" tab), in file order. Empty for non-kanji.
    QStringList containing(const QString &part);

private:
    /// Exactly one code point in the Han script (surrogate-aware).
    static bool isKanjiChar(const QString &ch);
    QString lastChar;
    QStringList lastLines;
};
