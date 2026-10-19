#include "core/recognizer/Recognizer.h"

#include "recognizer.hpp"

#include <QByteArray>
#include <QFile>

namespace {
/// Pen coords to bank points.
std::vector<std::vector<recognizer::StrokePoint>> toRaw(const QVector<QVector<QPointF>> &strokes) {
    std::vector<std::vector<recognizer::StrokePoint>> raw;
    raw.reserve(static_cast<std::size_t>(strokes.size()));
    for (const QVector<QPointF> &stroke : strokes) {
        std::vector<recognizer::StrokePoint> points;
        points.reserve(static_cast<std::size_t>(stroke.size()));
        for (const QPointF &p : stroke) {
            points.push_back(
                recognizer::StrokePoint{static_cast<float>(p.x()), static_cast<float>(p.y())});
        }
        raw.push_back(std::move(points));
    }
    return raw;
}

/// Ranked hits in Qt shape.
std::vector<Recognizer::Hit> toHits(const std::vector<recognizer::Candidate> &hits) {
    std::vector<Recognizer::Hit> out;
    for (const recognizer::Candidate &hit : hits) {
        out.push_back(Recognizer::Hit{
            QString::fromUtf8(hit.character.data(), static_cast<qsizetype>(hit.character.size())),
            hit.distance, hit.is_kana});
    }
    return out;
}
} // namespace

Recognizer &Recognizer::shared() {
    static Recognizer instance;
    return instance;
}

bool Recognizer::loadModel(const QString &path) {
    const std::lock_guard<std::mutex> lock(mutex);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        recognizer.reset();
        return false;
    }
    const QByteArray bytes = file.readAll();
    try {
        recognizer =
            recognizer::Recognizer::init(std::vector<std::uint8_t>(bytes.begin(), bytes.end()));
    } catch (const recognizer::RecognizerError &) {
        recognizer.reset();
        return false;
    }
    return true;
}

std::uint64_t Recognizer::templateCount() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return recognizer == nullptr ? 0 : recognizer->template_count();
}

bool Recognizer::hasModel() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return recognizer != nullptr;
}

std::vector<Recognizer::Hit> Recognizer::recognize(const QVector<QVector<QPointF>> &strokes,
                                                   std::uint32_t topK, bool includeKana) const {
    const std::lock_guard<std::mutex> lock(mutex);
    if (recognizer == nullptr) {
        return {};
    }
    return toHits(recognizer->recognize(toRaw(strokes), topK, includeKana));
}

std::vector<Recognizer::Hit> Recognizer::similar(const QVector<QVector<QPointF>> &strokes,
                                                 const QString &exclude, std::uint32_t topK) const {
    const std::lock_guard<std::mutex> lock(mutex);
    if (recognizer == nullptr) {
        return {};
    }
    return toHits(recognizer->similar(toRaw(strokes), exclude.toStdString(), topK));
}
