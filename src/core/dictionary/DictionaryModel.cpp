#include "core/dictionary/DictionaryModel.h"

#include "core/dictionary/DictWorker.h"
#include "dict.hpp"
#include "util/Romaji.h"

#include <QAbstractItemModel>
#include <QMetaType>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <Qt>
#include <QtGlobal>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

DictionaryModel::DictionaryModel(QObject *parent) : QAbstractTableModel(parent) {
    qRegisterMetaType<std::vector<dict::SearchHit>>();
    qRegisterMetaType<dict::SearchMode>();
    worker = new DictWorker;
    worker->moveToThread(&dictThread);
    connect(this, &DictionaryModel::requestLoad, worker, &DictWorker::load);
    connect(this, &DictionaryModel::requestSearch, worker, &DictWorker::search);
    connect(this, &DictionaryModel::requestSearchMeaning, worker, &DictWorker::searchMeaning);
    connect(worker, &DictWorker::loaded, this, &DictionaryModel::onLoaded);
    connect(worker, &DictWorker::searched, this, &DictionaryModel::onSearched);
    dictThread.start();
}

DictionaryModel::~DictionaryModel() {
    dictThread.quit();
    dictThread.wait();
    delete worker;
}

void DictionaryModel::loadAsync(const QString &path) {
    // Reloadable: scope switches swap the file, so every load
    // replaces the handle (the store swaps under a lock; in-flight
    // queries hold their own handle and land or drop by generation).
    // A new file invalidates everything in flight; fencing ready
    // routes queries typed mid-reload into pendingQuery (newest
    // wins) instead of searching the stale file.
    ++generation;
    ready = false;
    dictFile = path;
    emit requestLoad(path);
}

void DictionaryModel::search(const QString &query, dict::SearchMode mode) {
    if (!ready || query.trimmed().isEmpty()) {
        return;
    }
    ++generation;
    lastQuery = query;
    emit requestSearch(query, mode, hitCap, generation);
}

void DictionaryModel::searchMeaning(const QString &query) {
    if (!ready || query.trimmed().isEmpty()) {
        return;
    }
    ++generation;
    lastQuery = query;
    emit requestSearchMeaning(query, hitCap, generation);
}

void DictionaryModel::clear() {
    if (rows.empty()) {
        return;
    }
    // Bumping drops in-flight results (scope switches and fresh
    // queries both outrank them).
    ++generation;
    beginResetModel();
    rows.clear();
    cache.clear();
    lastQuery.clear();
    endResetModel();
    emit searchFinished(0, 0.0);
}

void DictionaryModel::refreshDisplay() {
    // Script conversion reads live state at `data` time, so only
    // the painted cells need a nudge (columns 0–1; tags and glosses
    // never change script).
    if (rows.empty()) {
        return;
    }
    emit dataChanged(index(0, 0), index(static_cast<int>(rows.size()) - 1, 1), {Qt::DisplayRole});
}

int DictionaryModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows.size());
}

int DictionaryModel::columnCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : 3;
}

QVariant DictionaryModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rows.size())) {
        return {};
    }
    const CachedRow &row = rowAt(index.row());
    if (role == Qt::UserRole) {
        return QVariant::fromValue(row.entSeq);
    }
    if (role == DictionaryModel::SensesRole) {
        return QVariant::fromValue(row.senses);
    }
    if (role == DictionaryModel::DeinflectedRole) {
        return !rows[static_cast<std::size_t>(index.row())].chain.empty();
    }
    if (role == DictionaryModel::InflectionRole) {
        QStringList reasons;
        for (const std::string &reason : rows[static_cast<std::size_t>(index.row())].chain) {
            reasons << QString::fromStdString(reason);
        }
        // zkanji joins stacked inflections with ", " (see its
        // wordInflectionText).
        return reasons.join(QStringLiteral(", "));
    }
    if (role == DictionaryModel::MatchRole) {
        const dict::SearchHit &hit = rows[static_cast<std::size_t>(index.row())];
        if (hit.chain.empty()) {
            return lastQuery;
        }
        return QString::fromStdString(hit.matched);
    }
    if (role == DictionaryModel::KanaOnlyRole) {
        // First sense only (zkanji marks defs[0] with KanaOnly when
        // it carries &uk; — "usually written using kana alone").
        return !row.senses.isEmpty() && row.senses.first().misc.contains(QStringLiteral("uk"));
    }
    if (role != Qt::DisplayRole) {
        return {};
    }
    switch (index.column()) {
    case 0:
        return row.headingIsKana ? Romaji::displayReading(row.heading) : row.heading;
    case 1:
        return Romaji::displayReading(row.reading);
    case 2:
        return row.meaning;
    default:
        return {};
    }
}

QVariant DictionaryModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
    case 0:
        return tr("Heading");
    case 1:
        return tr("Reading");
    case 2:
        return tr("Meaning");
    default:
        return {};
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void DictionaryModel::onLoaded(bool ok, quint64 total) {
    if (!ok) {
        emit loaded(false);
        return;
    }
    // Handles live in the store (loaded on the worker thread);
    // this side only records readiness.
    entries = total;
    ready = true;
    emit loaded(true);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void DictionaryModel::onSearched(std::vector<dict::SearchHit> hitsIn, double searchMs,
                                 quint64 generationIn) {
    if (generationIn != generation) {
        return; // superseded mid-flight: a newer query owns the rows
    }
    beginResetModel();
    rows = std::move(hitsIn);
    cache.assign(rows.size(), std::nullopt);
    endResetModel();
    emit searchFinished(static_cast<int>(rows.size()), searchMs);
}
