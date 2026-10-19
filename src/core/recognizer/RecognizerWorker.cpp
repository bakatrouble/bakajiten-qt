#include "core/recognizer/RecognizerWorker.h"

#include "core/dictionary/DictStore.h"

#include <QDebug>
#include <QElapsedTimer>

namespace {
/// Similar-strip cap.
constexpr uint32_t similarShown = 12;
} // namespace

RecognizerWorker::RecognizerWorker(QObject *parent) : QObject(parent) {}

void RecognizerWorker::loadModel(const QString &path) {
    auto &shared = Recognizer::shared();
    const bool ok = shared.loadModel(path);
    emit modelLoaded(path, ok, shared.templateCount());
}

void RecognizerWorker::recognize(QVector<QVector<QPointF>> strokes, quint32 topK, bool includeKana,
                                 quint64 generation) {
    QElapsedTimer timer;
    timer.start();
    const std::vector<Recognizer::Hit> hits =
        Recognizer::shared().recognize(strokes, topK, includeKana);
    emit recognized(hits, static_cast<double>(timer.nsecsElapsed()) / 1e6, generation);
}

void RecognizerWorker::loadSimilar(const QString &ch, quint64 generation) {
    std::vector<Recognizer::Hit> hits;
    try {
        for (const Recognizer::Hit &hit : DictStore::shared().similarKanji(ch, similarShown)) {
            hits.push_back(hit);
        }
    } catch (...) {
        // Same policy as the match path: corrupt data yields
        // nothing, never unwinds.
        qWarning() << "RecognizerWorker: similar load failed";
    }
    emit similarLoaded(hits, generation);
}
