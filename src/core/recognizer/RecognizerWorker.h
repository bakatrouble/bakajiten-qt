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

/// Match worker: scores through the shared `Recognizer` (global
/// template bank) on a dedicated QThread, so scoring never blocks
/// the UI thread. All queries go through these queued slots;
/// results come back with the request's generation id, and the
/// widget drops anything stale (strokes changed mid-flight).
/// Strokes cross by value: QVector shares implicitly, and the
/// worker only const-reads its copy, so no lock is needed.
class RecognizerWorker : public QObject {
    Q_OBJECT
public:
    explicit RecognizerWorker(QObject *parent = nullptr);

public slots:
    /// Decode the bank at `path` (bank cleared on failure).
    void loadModel(const QString &path);
    /// Score finished pen strokes (best first, at most `topK`).
    void recognize(QVector<QVector<QPointF>> strokes, quint32 topK, bool includeKana,
                   quint64 generation);
    /// Shape-similar kanji for one character (empty when the bank
    /// is missing or nothing matches).
    void loadSimilar(const QString &ch, quint64 generation);

signals:
    /// Bank load settled (ok = decoded, with its template count).
    void modelLoaded(const QString &path, bool ok, quint64 templateCount);
    /// Scored hits with their match time, echoing the request id.
    void recognized(std::vector<Recognizer::Hit> hits, double matchMs, quint64 generation);
    /// Similar shapes, echoing the request id.
    void similarLoaded(std::vector<Recognizer::Hit> hits, quint64 generation);
};
