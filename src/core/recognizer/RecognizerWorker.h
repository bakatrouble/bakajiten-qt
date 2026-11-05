#pragma once

#include "core/recognizer/Recognizer.h"

#include <QMetaType>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QVector>

#include <cstdint>
#include <vector>

// Queued request/result payloads (registered at runtime in the
// widget ctor): nested stroke vectors in, ranked hits out.
Q_DECLARE_METATYPE(QVector<QVector<QPointF>>)

/// Match worker: scores through the shared `Recognizer` (one loaded
/// Japanese pack) on a dedicated QThread, so scoring never blocks
/// the UI thread. Model downloads never happen here — the
/// Dictionaries dialog fetches the pack through the data
/// downloader. All queries go through these queued slots; results
/// come back with the request's generation id, and the widget drops
/// anything stale (strokes changed mid-flight). Strokes cross by
/// value: QVector shares implicitly, and the worker only const-reads
/// its copy, so no lock is needed.
class RecognizerWorker : public QObject {
    Q_OBJECT
public:
    explicit RecognizerWorker(QObject *parent = nullptr);

public slots:
    /// Load the cached pack (no download; fails when missing).
    void loadModel();
    /// Score finished pen strokes (best first, at most `topK`).
    void recognize(QVector<QVector<QPointF>> strokes, quint32 topK, quint64 generation);

signals:
    /// Pack load settled (ok = decoded, with LM presence).
    void modelLoaded(bool ok, bool hasLanguageModel);
    /// Scored hits with their match time, echoing the request id.
    void recognized(std::vector<Recognizer::Hit> hits, double matchMs, quint64 generation);
};
