#include "core/dictionary/DictWorker.h"
#include "core/dictionary/DictStore.h"

#include <QDebug>
#include <QElapsedTimer>

DictWorker::DictWorker(QObject *parent) : QObject(parent) {}

void DictWorker::load(const QString &path) {
    if (!DictStore::shared().loadDictionary(path)) {
        emit loaded(false, 0);
        return;
    }
    emit loaded(true, DictStore::shared().entryCount());
}

void DictWorker::search(QString query, dict::SearchMode mode, quint32 cap, quint64 generation) {
    QElapsedTimer timer;
    timer.start();
    std::vector<dict::SearchHit> hits =
        DictStore::shared().searchIndices(query.toStdString(), mode, cap);
    emit searched(hits, static_cast<double>(timer.nsecsElapsed()) / 1e6, generation);
}

void DictWorker::searchMeaning(QString query, quint32 cap, quint64 generation) {
    QElapsedTimer timer;
    timer.start();
    std::vector<dict::SearchHit> hits =
        DictStore::shared().searchMeaningIndices(query.toStdString(), cap);
    emit searched(hits, static_cast<double>(timer.nsecsElapsed()) / 1e6, generation);
}

void DictWorker::loadParts(const QString &ch, quint64 generation) {
    QStringList parts;
    try {
        if (const std::optional<dict::KanjiEntry> entry =
                DictStore::shared().lookupKanji(ch.toStdString())) {
            for (const std::string &part : entry->parts) {
                const QString text = QString::fromStdString(part);
                // kradfile lists atomic characters as their own
                // sole part.
                if (text != ch) {
                    parts << text;
                }
            }
        }
    } catch (...) {
        qWarning() << "DictWorker: parts load failed";
    }
    emit partsLoaded(parts, generation);
}

void DictWorker::loadPartOf(const QString &ch, quint64 generation) {
    QStringList partOf;
    try {
        for (const std::string &user : DictStore::shared().kanjiContaining(ch.toStdString())) {
            partOf << QString::fromStdString(user);
        }
    } catch (...) {
        qWarning() << "DictWorker: part-of load failed";
    }
    emit partOfLoaded(partOf, generation);
}
