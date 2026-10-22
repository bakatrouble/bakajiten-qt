#include "core/state/SettingsStore.h"

#include <rpp/operators/distinct_until_changed.hpp>
#include <rpp/operators/observe_on.hpp>
#include <rpp/operators/skip.hpp>
#include <rpp/operators/subscribe.hpp>
#include <rppqt/rppqt.hpp>

#include <QPointer>

#include <functional>

SettingsStore &SettingsStore::shared() {
    static SettingsStore instance;
    return instance;
}

/// Process settings accessor (kept for read/persist call sites
/// while they migrate). Points into the store's owned value, so
/// direct writes land silently — production writers use set().
AppSettings &AppSettings::shared() { return SettingsStore::shared().valueRef(); }

SettingsStore::SettingsStore() : live(AppSettings::fromStorage()), state(live) {}

AppSettings SettingsStore::current() const { return live; }

AppSettings &SettingsStore::valueRef() { return live; }

void SettingsStore::set(const AppSettings &settings) {
    if (settings == live) {
        return;
    }
    live = settings;
    state.get_observer().on_next(settings);
}

void SettingsStore::subscribe(QObject *context, Handler handler) {
    state.get_observable() | rpp::operators::skip(1) |
        rpp::operators::distinct_until_changed(std::equal_to<AppSettings>{}) |
        rpp::operators::observe_on(rppqt::schedulers::main_thread_scheduler{}) |
        rpp::operators::subscribe(
            [guard = QPointer<QObject>(context), handler](const AppSettings &settings) {
                if (!guard.isNull()) {
                    handler(settings);
                }
            });
}
