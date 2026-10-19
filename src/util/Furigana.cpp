#include "util/Furigana.h"

#include "dict.hpp"

#include <QVector>

#include <optional>

std::optional<QVector<Furigana::Span>> Furigana::segment(const QString &head,
                                                         const QString &reading) {
    // Segmentation lives in Rust (same traces); Qt only converts.
    const auto spans = dict::furigana_segment(head.toStdString(), reading.toStdString());
    if (!spans.has_value()) {
        return std::nullopt;
    }
    QVector<Span> out;
    out.reserve(static_cast<int>(spans->size()));
    for (const dict::FuriganaSpan &span : *spans) {
        out << Span{QString::fromStdString(span.text), QString::fromStdString(span.ruby)};
    }
    return out;
}
