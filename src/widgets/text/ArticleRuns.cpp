#include "widgets/text/ArticleRuns.h"

#include "core/dictionary/DictionaryModel.h"
#include "util/Furigana.h"
#include "util/Romaji.h"
#include "widgets/text/RubyLabel.h"
#include "widgets/text/RunLabel.h"

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace {
QString displayHeadword(const QString &heading) {
    return Romaji::isKana(heading) ? Romaji::displayReading(heading) : heading;
}
} // namespace

QVector<RubyLabel::Span> ArticleRuns::headingSpans(const ArticleDisplay &article) {
    const auto segmented = Furigana::segment(article.heading, article.reading);
    QVector<RubyLabel::Span> spans;
    if (segmented.has_value()) {
        for (const Furigana::Span &span : *segmented) {
            spans << RubyLabel::Span{span.hasRuby() ? span.text : Romaji::displayReading(span.text),
                                     span.hasRuby() ? Romaji::displayReading(span.ruby) : QString(),
                                     span.text};
        }
        return spans;
    }
    spans << RubyLabel::Span{displayHeadword(article.heading), {}, article.heading};
    return spans;
}

std::optional<QString> ArticleRuns::readingLine(const ArticleDisplay &article) {
    QString reading = Romaji::displayReading(article.reading);
    if (reading == displayHeadword(article.heading)) {
        return std::nullopt;
    }
    return reading;
}

QVector<RunLabel::Run> ArticleRuns::senseRuns(const SenseDisplay &sense, int number) {
    QVector<RunLabel::Run> runs = {
        {QStringLiteral("%1. ").arg(number), RunLabel::Ink::Body, false, true, {}}};
    // One run per tag (concatenated text identical to the old
    // joined run); each run carries only its own description.
    const auto appendTags = [&](const QStringList &labels, const QStringList &tips,
                                RunLabel::Ink ink, const QString &lead, const QString &trail) {
        for (int i = 0; i < labels.size(); ++i) {
            const QString text = (i == 0 ? lead : QString()) + labels[i] +
                                 (i + 1 < labels.size() ? QStringLiteral(", ") : trail);
            const QString tip = i < tips.size() ? tips[i] : QString();
            runs << RunLabel::Run{text, ink, true, false, {}, tip};
        }
    };
    if (!sense.pos.isEmpty()) {
        appendTags(sense.pos, sense.posTips, RunLabel::Ink::Green, QString(), QStringLiteral(" "));
    }
    if (!sense.misc.isEmpty()) {
        // Per-tag runs, like the table: only `uk` reads
        // warning-red (displayed as `kana`), the rest stay dim.
        for (int i = 0; i < sense.misc.size(); ++i) {
            const QString separator =
                i + 1 < sense.misc.size() ? QStringLiteral(", ") : QStringLiteral(" ");
            const RunLabel::Ink ink =
                isUsuallyKana(sense.misc[i]) ? RunLabel::Ink::Red : RunLabel::Ink::Dim;
            const QString shown =
                ink == RunLabel::Ink::Red ? QStringLiteral("kana") : sense.misc[i];
            const QString tip = i < sense.miscTips.size() ? sense.miscTips[i] : QString();
            runs << RunLabel::Run{shown + separator, ink, true, false, {}, tip};
        }
    }
    runs << RunLabel::Run{sense.gloss, RunLabel::Ink::Body};
    if (!sense.field.isEmpty()) {
        appendTags(sense.field, sense.fieldTips, RunLabel::Ink::Link, QStringLiteral(" "),
                   QString());
    }
    const auto metaLinks = [&](const QString &title, const QStringList &terms) {
        runs << RunLabel::Run{
            QStringLiteral("\n%1: ").arg(title), RunLabel::Ink::Dim, true, false, {}};
        bool first = true;
        for (const QString &term : terms) {
            if (!first) {
                runs << RunLabel::Run{QStringLiteral(", "), RunLabel::Ink::Dim, true, false, {}};
            }
            first = false;
            const QString query = queryFor(term);
            if (query.isEmpty()) {
                runs << RunLabel::Run{term, RunLabel::Ink::Dim, true, false, {}};
            } else {
                runs << RunLabel::Run{term, RunLabel::Ink::Link, true, false, query};
            }
        }
    };
    if (!sense.xref.isEmpty()) {
        metaLinks(QCoreApplication::translate("DictionaryWidget", "See also"), sense.xref);
    }
    if (!sense.ant.isEmpty()) {
        metaLinks(QCoreApplication::translate("DictionaryWidget", "Antonyms"), sense.ant);
    }
    if (!sense.onlyKeb.isEmpty() || !sense.onlyReb.isEmpty()) {
        metaLinks(QCoreApplication::translate("DictionaryWidget", "Only"),
                  sense.onlyKeb + sense.onlyReb);
    }
    for (const QString &note : sense.notes) {
        runs << RunLabel::Run{
            QStringLiteral("\n%1").arg(note), RunLabel::Ink::Dim, true, false, {}};
    }
    for (const SourceDisplay &source : sense.sources) {
        runs << RunLabel::Run{QStringLiteral("\nFrom %1")
                                  .arg(source.lang.isEmpty()
                                           ? source.text
                                           : source.lang + QStringLiteral(": ") + source.text),
                              RunLabel::Ink::Dim,
                              true,
                              false,
                              {}};
    }
    return runs;
}

QVector<ArticleRuns::FormBlock> ArticleRuns::formBlocks(const ArticleDisplay &article) {
    QVector<FormBlock> blocks;
    const auto elementNotes = [](const ElementDisplay &element) {
        QStringList notes = element.info;
        if (element.noKanji) {
            notes << QCoreApplication::translate("DictionaryWidget", "no kanji");
        }
        if (!element.restrictedTo.isEmpty()) {
            notes << QCoreApplication::translate("DictionaryWidget", "only: %1")
                         .arg(element.restrictedTo.join(QStringLiteral(", ")));
        }
        return notes.join(QStringLiteral("; "));
    };
    // Kanji forms segment against the article reading (the reading
    // shown above); restricted readings that do not align fall back
    // to plain text, like kana forms always do.
    const auto addForms = [&](const QVector<ElementDisplay> &elements, bool isReading,
                              const QString &skip) {
        for (int i = 0; i < elements.size(); ++i) {
            const ElementDisplay &element = elements[i];
            if (i == 0 && element.form == skip) {
                continue;
            }
            FormBlock block;
            if (!isReading) {
                if (const auto spans = Furigana::segment(element.form, article.reading);
                    spans.has_value()) {
                    block.ruby = true;
                    for (const Furigana::Span &span : *spans) {
                        block.spans << RubyLabel::Span{
                            span.hasRuby() ? span.text : Romaji::displayReading(span.text),
                            span.hasRuby() ? Romaji::displayReading(span.ruby) : QString(),
                            span.text};
                    }
                    block.notes = elementNotes(element);
                    blocks << block;
                    continue;
                }
            }
            block.plain = isReading ? Romaji::displayReading(element.form) : element.form;
            const QString notes = elementNotes(element);
            block.plain =
                notes.isEmpty() ? block.plain : QStringLiteral("%1 (%2)").arg(block.plain, notes);
            blocks << block;
        }
    };
    addForms(article.kanji, false, article.heading);
    addForms(article.readings, true, article.reading);
    return blocks;
}

QString ArticleRuns::queryFor(const QString &term) { return term.section(u'・', 0, 0); }

bool ArticleRuns::isCommonWord(const ArticleDisplay &article) {
    static const QStringList common = {QStringLiteral("news1"), QStringLiteral("news2"),
                                       QStringLiteral("ichi1"), QStringLiteral("ichi2")};
    const auto hasCommon = [&](const QVector<ElementDisplay> &elements) {
        for (const ElementDisplay &element : elements) {
            for (const QString &code : element.priorities) {
                if (common.contains(code)) {
                    return true;
                }
            }
        }
        return false;
    };
    return hasCommon(article.kanji) || hasCommon(article.readings);
}
