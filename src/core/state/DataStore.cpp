#include "core/state/DataStore.h"

#include <rpp/operators/observe_on.hpp>
#include <rpp/operators/skip.hpp>
#include <rpp/operators/subscribe.hpp>
#include <rppqt/rppqt.hpp>

#include <QPointer>

namespace {
DataStore::Snapshot withNotice(DataStore::Snapshot snapshot, const QString &notice) {
    snapshot.notice = notice;
    return snapshot;
}
} // namespace

DataStore &DataStore::shared() {
    static DataStore instance;
    return instance;
}

DataStore::DataStore() : state(Snapshot{}) {}

DataStore::Snapshot DataStore::current() const { return state.get_value(); }

void DataStore::setRemote(const DataManifest &remote, const QByteArray &bytes) {
    Snapshot next = state.get_value();
    next.remote = remote;
    next.remoteBytes = bytes;
    state.get_observer().on_next(next);
}

void DataStore::clearRemote() {
    Snapshot next = state.get_value();
    next.remote.reset();
    next.remoteBytes.clear();
    state.get_observer().on_next(next);
}

void DataStore::setNotice(const QString &notice) {
    state.get_observer().on_next(withNotice(state.get_value(), notice));
}

void DataStore::markBusy(const QString &file) {
    Snapshot next = state.get_value();
    next.busy.insert(file);
    state.get_observer().on_next(next);
}

void DataStore::markIdle(const QString &file) {
    Snapshot next = state.get_value();
    next.busy.remove(file);
    next.received.remove(file);
    next.total.remove(file);
    state.get_observer().on_next(next);
}

void DataStore::clearBusy() {
    Snapshot next = state.get_value();
    next.busy.clear();
    next.received.clear();
    next.total.clear();
    state.get_observer().on_next(next);
}

void DataStore::setProgress(const QString &file, qint64 received, qint64 total) {
    Snapshot next = state.get_value();
    next.busy.insert(file);
    next.received.insert(file, received);
    next.total.insert(file, total);
    state.get_observer().on_next(next);
}

void DataStore::clearProgress(const QString &file) {
    Snapshot next = state.get_value();
    next.received.remove(file);
    next.total.remove(file);
    state.get_observer().on_next(next);
}

void DataStore::refresh() { state.get_observer().on_next(state.get_value()); }

void DataStore::subscribe(QObject *context, Handler handler) {
    state.get_observable() | rpp::operators::skip(1) |
        rpp::operators::observe_on(rppqt::schedulers::main_thread_scheduler{}) |
        rpp::operators::subscribe(
            [guard = QPointer<QObject>(context), handler](const Snapshot &snapshot) {
                if (!guard.isNull()) {
                    handler(snapshot);
                }
            });
}
