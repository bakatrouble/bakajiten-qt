#include "core/recognizer/Recognizer.h"

#include "recognizer.hpp"

#include "core/data/DataPaths.h"
#include "core/recognizer/ModelPack.h"

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
            QString::fromUtf8(hit.text.data(), static_cast<qsizetype>(hit.text.size())),
            hit.score});
    }
    return out;
}

/// Read one artifact file; empty when missing/unreadable.
std::vector<std::uint8_t> readBytes(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray bytes = file.readAll();
    return std::vector<std::uint8_t>(bytes.begin(), bytes.end());
}
} // namespace

Recognizer &Recognizer::shared() {
    static Recognizer instance;
    return instance;
}

bool Recognizer::loadModel() {
    const std::lock_guard<std::mutex> lock(mutex);
    return loadLocked();
}

void Recognizer::reset() {
    const std::lock_guard<std::mutex> lock(mutex);
    recognizer.reset();
}

bool Recognizer::loadLocked() {
    try {
        const std::vector<std::uint8_t> bundle = readBytes(ModelPacks::safetensorsPath());
        if (bundle.empty()) {
            recognizer.reset();
            return false;
        }
        recognizer = recognizer::Recognizer::init(bundle);
    } catch (const recognizer::RecognizerError &) {
        recognizer.reset();
        return false;
    }
    return recognizer != nullptr;
}

bool Recognizer::hasModel() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return recognizer != nullptr;
}

bool Recognizer::hasLanguageModel() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return recognizer != nullptr && recognizer->has_language_model();
}

std::vector<Recognizer::Hit> Recognizer::recognize(const QVector<QVector<QPointF>> &strokes,
                                                   std::uint32_t topK) const {
    std::shared_ptr<recognizer::Recognizer> handle;
    {
        const std::lock_guard<std::mutex> lock(mutex);
        handle = recognizer;
    }
    if (handle == nullptr) {
        return {};
    }
    return toHits(handle->recognize(toRaw(strokes), topK));
}
