#include "core/state/SearchStore.h"

#include <rpp/operators/debounce.hpp>
#include <rpp/operators/distinct_until_changed.hpp>
#include <rpp/operators/filter.hpp>
#include <rpp/operators/skip.hpp>
#include <rpp/operators/subscribe.hpp>
#include <rpp/operators/with_latest_from.hpp>
#include <rppqt/rppqt.hpp>

#include <QPointer>

#include <chrono>
#include <functional>

namespace {
/// Live-search delay (matches the old QTimer interval).
constexpr std::chrono::milliseconds kDebounce{200};

SearchStore::Request makeRequest(SearchStore::Trigger trigger, const QString &text,
                                 const SearchStore::Flags &flags, int scope) {
    SearchStore::Request request;
    request.text = text;
    request.mode = flags.mode;
    request.meaning = flags.meaning;
    request.scope = scope;
    request.trigger = trigger;
    return request;
}
} // namespace

SearchStore &SearchStore::shared() {
    static SearchStore instance;
    return instance;
}

SearchStore::SearchStore() : texts(QString()), flags(Flags{}), scopes(0) {}

void SearchStore::setText(const QString &text) {
    if (text == texts.get_value()) {
        return;
    }
    texts.get_observer().on_next(text);
}

void SearchStore::setFlags(dict::SearchMode mode, bool meaning) {
    const Flags next{mode, meaning};
    if (next == flags.get_value()) {
        return;
    }
    flags.get_observer().on_next(next);
}

void SearchStore::setScope(int scope) {
    if (scope == scopes.get_value()) {
        return;
    }
    scopes.get_observer().on_next(scope);
}

QString SearchStore::text() const { return texts.get_value(); }

SearchStore::Flags SearchStore::criteria() const { return flags.get_value(); }

int SearchStore::scope() const { return scopes.get_value(); }

void SearchStore::subscribe(QObject *context, Handler handler) {
    auto guarded = [guard = QPointer<QObject>(context), handler](const Request &request) {
        if (!guard.isNull()) {
            handler(request);
        }
    };
    const auto textObs = texts.get_observable();
    const auto flagsObs = flags.get_observable();
    const auto scopesObs = scopes.get_observable();
    // Live text: debounced on the Qt main thread (the rppqt
    // scheduler posts through QTimer, so this fires on the GUI
    // thread like the old QTimer path did).
    textObs | rpp::operators::skip(1) |
        rpp::operators::distinct_until_changed(std::equal_to<QString>{}) |
        rpp::operators::filter([](const QString &text) { return !text.trimmed().isEmpty(); }) |
        rpp::operators::debounce(kDebounce, rppqt::schedulers::main_thread_scheduler{}) |
        rpp::operators::with_latest_from(
            [](const QString &text, const Flags &flags, int scope) {
                return makeRequest(Trigger::Text, text, flags, scope);
            },
            flagsObs, scopesObs) |
        rpp::operators::subscribe(guarded);
    // Emptied field: immediate clear (the old path stopped the
    // timer and cleared synchronously — no debounce here).
    textObs | rpp::operators::skip(1) |
        rpp::operators::filter([](const QString &text) { return text.trimmed().isEmpty(); }) |
        rpp::operators::with_latest_from(
            [](const QString &text, const Flags &flags, int scope) {
                return makeRequest(Trigger::Text, text, flags, scope);
            },
            flagsObs, scopesObs) |
        rpp::operators::subscribe(guarded);
    // Edge mode / target switch: immediate re-search.
    flagsObs | rpp::operators::skip(1) |
        rpp::operators::distinct_until_changed(std::equal_to<Flags>{}) |
        rpp::operators::with_latest_from(
            [](const Flags &flags, const QString &text, int scope) {
                return makeRequest(Trigger::Flags, text, flags, scope);
            },
            textObs, scopesObs) |
        rpp::operators::subscribe(guarded);
    // Scope switch: immediate file swap downstream.
    scopesObs | rpp::operators::skip(1) |
        rpp::operators::distinct_until_changed(std::equal_to<int>{}) |
        rpp::operators::with_latest_from(
            [](int scope, const QString &text, const Flags &flags) {
                return makeRequest(Trigger::Scope, text, flags, scope);
            },
            textObs, flagsObs) |
        rpp::operators::subscribe(guarded);
}
