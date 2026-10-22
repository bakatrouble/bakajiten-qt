#pragma once

#include "dict.hpp"

#include <rpp/subjects/behavior_subject.hpp>

#include <QObject>
#include <QString>

#include <cstdint>
#include <functional>

/// Search inputs + the processed request stream (singleton).
///
/// Owns what DictionaryWidget kept as members: the edge mode, the
/// Japanese/meaning target, the scope index, and the live field
/// text. Widgets publish raw UI events here (field text, edge
/// toggles, target switch, scope combo); the store debounces the
/// text path and routes every change to subscribers as one
/// Request. State reads (`text`, `criteria`, `scope`) re-sync the
/// controls after dev `.ui` reloads rebuild the widgets.
class SearchStore {
public:
    /// What changed: raw field text (debounced), edge/target
    /// flags (immediate), or scope (immediate — the subscriber
    /// swaps the loaded file for these).
    enum class Trigger : std::uint8_t { Text, Flags, Scope };

    struct Request {
        QString text;
        dict::SearchMode mode = dict::SearchMode::kPrefix;
        bool meaning = false;
        int scope = 0;
        Trigger trigger = Trigger::Text;
    };

    /// Edge mode + search target (one subject, one emission).
    struct Flags {
        dict::SearchMode mode = dict::SearchMode::kPrefix;
        bool meaning = false;
        bool operator==(const Flags &other) const = default;
    };

    using Handler = std::function<void(const Request &)>;

    /// Process singleton (leaked at exit — the app always needs it).
    static SearchStore &shared();

    SearchStore(const SearchStore &) = delete;
    SearchStore &operator=(const SearchStore &) = delete;

    void setText(const QString &text);
    void setFlags(dict::SearchMode mode, bool meaning);
    void setScope(int scope);

    QString text() const;
    Flags criteria() const;
    int scope() const;

    /// Processed requests (text debounced 200 ms like the old
    /// QTimer, flags/scope immediate). Guarded by `context`:
    /// destroyed widgets never hear back, so no unsubscribe call.
    void subscribe(QObject *context, Handler handler);

private:
    SearchStore();

    rpp::subjects::behavior_subject<QString> texts;
    rpp::subjects::behavior_subject<Flags> flags;
    rpp::subjects::behavior_subject<int> scopes;
};
