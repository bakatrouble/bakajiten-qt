#pragma once

#include "widgets/text/RubyLabel.h"
#include "widgets/text/RunLabel.h"

#include <QString>
#include <QVector>

#include <optional>

struct ArticleDisplay;
struct SenseDisplay;
struct ElementDisplay;

/// Article data shaping: model structs to label runs and ruby
/// spans. No widgets — `ArticleView` assembles these. Display
/// transliteration (Romaji setting) applies to kana runs and
/// readings here, never to kanji bases.
namespace ArticleRuns {
/// Heading ruby spans (segmented) or a single plain span.
QVector<RubyLabel::Span> headingSpans(const ArticleDisplay &article);
/// Reading line, or nullopt when identical to the heading.
std::optional<QString> readingLine(const ArticleDisplay &article);
/// One sense as run blocks (number, tags, gloss, meta lines).
QVector<RunLabel::Run> senseRuns(const SenseDisplay &sense, int number);
/// One remaining headword form: ruby spans or plain text, plus
/// parenthesized notes.
struct FormBlock {
    bool ruby = false;
    QVector<RubyLabel::Span> spans;
    QString plain;
    QString notes;
};
QVector<FormBlock> formBlocks(const ArticleDisplay &article);
/// Meta-reference query: display keeps the `・n` tail, lookup
/// drops it.
QString queryFor(const QString &term);
/// JMdict frequency convention behind the "common word" badge.
bool isCommonWord(const ArticleDisplay &article);
} // namespace ArticleRuns
