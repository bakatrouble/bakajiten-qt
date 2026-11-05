#include "core/recognizer/RecognizerWorker.h"

#include <QElapsedTimer>

RecognizerWorker::RecognizerWorker(QObject *parent) : QObject(parent) {}

void RecognizerWorker::loadModel() {
    auto &shared = Recognizer::shared();
    const bool ok = shared.loadModel();
    emit modelLoaded(ok, ok && shared.hasLanguageModel());
}

void RecognizerWorker::recognize(QVector<QVector<QPointF>> strokes, quint32 topK,
                                 quint64 generation) {
    // No model, no download: the Dictionaries dialog is the only
    // surface that fetches the pack.
    if (!Recognizer::shared().hasModel()) {
        emit recognized({}, 0.0, generation);
        return;
    }
    QElapsedTimer timer;
    timer.start();
    const std::vector<Recognizer::Hit> hits = Recognizer::shared().recognize(strokes, topK);
    emit recognized(hits, static_cast<double>(timer.nsecsElapsed()) / 1e6, generation);
}
