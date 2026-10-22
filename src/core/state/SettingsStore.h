#pragma once

#include "core/settings/Settings.h"

#include <rpp/subjects/behavior_subject.hpp>

#include <QObject>
#include <QString>

#include <functional>

/// Live appearance + interface settings (singleton).
///
/// Owns the process value, initialized from storage;
/// AppSettings stays the persistence + apply mechanics (`save`,
/// `apply`, `fromStorage`). Writers go through `set` so
/// subscribers hear about changes on the Qt main thread;
/// `AppSettings::shared()` reads the same value for call sites
/// that only need the current one.
class SettingsStore {
public:
    using Handler = std::function<void(const AppSettings &)>;

    /// Process singleton (leaked at exit — the app always needs it).
    static SettingsStore &shared();

    SettingsStore(const SettingsStore &) = delete;
    SettingsStore &operator=(const SettingsStore &) = delete;

    AppSettings current() const;
    /// Publish a new value (persistence still explicit: callers
    /// save through AppSettings like before).
    void set(const AppSettings &settings);
    /// Direct access to the owned value (bypasses publication —
    /// AppSettings::shared() compatibility and persistence only;
    /// production writers use set()).
    AppSettings &valueRef();
    /// Change notifications, guarded by `context` (no unsubscribe).
    void subscribe(QObject *context, Handler handler);

private:
    SettingsStore();

    /// Owned value (the subject mirrors it; the reference
    /// AppSettings::shared() hands out points here, so legacy
    /// direct writes land in the store too — without publication).
    AppSettings live;
    rpp::subjects::behavior_subject<AppSettings> state;
};
