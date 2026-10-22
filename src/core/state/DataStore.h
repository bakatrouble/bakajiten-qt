#pragma once

#include "core/data/DataManifest.h"

#include <rpp/subjects/behavior_subject.hpp>

#include <QByteArray>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>

#include <functional>
#include <optional>

/// Dictionaries-dialog state (singleton): the last catalog check,
/// in-flight transfers with per-file progress, and the status line
/// text. The dialog stays a thin view — check/download effects
/// forward here and rendering subscribes — so reopening it keeps
/// the last known remote instead of re-checking, and any future
/// surface (startup flow, settings) reads the same snapshot.
class DataStore {
public:
    struct Snapshot {
        std::optional<DataManifest> remote;
        QByteArray remoteBytes;
        QSet<QString> busy;
        /// Per-file wire progress (received/total bytes).
        QMap<QString, qint64> received;
        QMap<QString, qint64> total;
        QString notice;
    };

    using Handler = std::function<void(const Snapshot &)>;

    /// Process singleton (leaked at exit — the app always needs it).
    static DataStore &shared();

    DataStore(const DataStore &) = delete;
    DataStore &operator=(const DataStore &) = delete;

    Snapshot current() const;
    void setRemote(const DataManifest &remote, const QByteArray &bytes);
    void clearRemote();
    /// Status line text (checking…, as-of date, failures).
    void setNotice(const QString &notice);
    void markBusy(const QString &file);
    void markIdle(const QString &file);
    void clearBusy();
    void setProgress(const QString &file, qint64 received, qint64 total);
    void clearProgress(const QString &file);
    /// Re-emit the current snapshot (disk changed under us —
    /// removes and clears read the files directly).
    void refresh();
    /// State snapshots on the Qt main thread, guarded by
    /// `context` (no unsubscribe).
    void subscribe(QObject *context, Handler handler);

private:
    DataStore();

    /// Serialized: progress lands from network callbacks while
    /// actions publish from slots, so emissions never interleave.
    rpp::subjects::serialized_behavior_subject<Snapshot> state;
};
