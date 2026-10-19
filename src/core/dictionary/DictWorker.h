#pragma once

#include "dict.hpp"

#include <QMetaType>
#include <QObject>
#include <QString>

#include <cstdint>
#include <memory>
#include <vector>

#pragma once

#include "dict.hpp"

#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <cstdint>
#include <memory>
#include <vector>

Q_DECLARE_METATYPE(std::vector<dict::SearchHit>)

/// Dictionary worker: index lookups (plus kanji loads) on a
/// dedicated QThread, so artifact loading and lookups never block
/// the UI thread. Search returns index hits only (file flag +
/// record index) — entry contents stay unread until rows scroll
/// into view (see `DictionaryModel`). Results echo the request's
/// generation id; callers drop anything stale.
class DictWorker : public QObject {
    Q_OBJECT
public:
    explicit DictWorker(QObject *parent = nullptr);

public slots:
    /// Map one dictionary artifact and build the handle
    /// (failures report through `loaded` with a zero count).
    void load(const QString &path);
    /// Index lookup over the loaded file, `cap` total.
    void search(QString query, dict::SearchMode mode, quint32 cap, quint64 generation);
    /// English gloss substring lookup (same `searched` shape —
    /// rows materialize identically either way).
    void searchMeaning(QString query, quint32 cap, quint64 generation);
    /// kradfile parts for one character (atomic characters have
    /// none — the caller unchecks).
    void loadParts(const QString &ch, quint64 generation);
    /// Characters using one character as a part, in file order.
    void loadPartOf(const QString &ch, quint64 generation);

signals:
    /// Load settled (ok = artifact decoded).
    void loaded(bool ok, quint64 entries);
    /// Index hits with their match time, echoing the request id.
    void searched(std::vector<dict::SearchHit> hits, double searchMs, quint64 generation);
    /// kradfile parts, echoing the request id.
    void partsLoaded(QStringList parts, quint64 generation);
    /// Part-of list, echoing the request id.
    void partOfLoaded(QStringList partOf, quint64 generation);
};
