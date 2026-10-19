#include "core/dictionary/DictStore.h"

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStringList>

DictStore &DictStore::shared() {
    static DictStore instance;
    return instance;
}

bool DictStore::loadDictionary(const QString &path) {
    if (path.isEmpty()) {
        return false;
    }
    // Mmapped inside the handle: instant, no copy, shared page
    // cache. Swaps the live handle, so scope switches reload.
    std::shared_ptr<dict::Dictionary> loaded;
    try {
        loaded = dict::Dictionary::from_path(path.toStdString());
    } catch (const dict::DictError &) {
        const std::lock_guard lock(mutex);
        dictionary.reset();
        return false;
    }
    const std::lock_guard lock(mutex);
    dictionary = std::move(loaded);
    return true;
}

bool DictStore::loadKanji(const QString &kanjiPath) {
    if (kanjiPath.isEmpty()) {
        return false;
    }
    std::shared_ptr<dict::KanjiDictionary> loaded;
    try {
        loaded = dict::KanjiDictionary::from_path(kanjiPath.toStdString());
    } catch (const dict::DictError &) {
        const std::lock_guard lock(mutex);
        kanji.reset();
        return false;
    }
    const std::lock_guard lock(mutex);
    kanji = std::move(loaded);
    return true;
}

bool DictStore::dictionaryReady() const {
    const std::shared_lock lock(mutex);
    return dictionary != nullptr;
}

bool DictStore::kanjiReady() const {
    const std::shared_lock lock(mutex);
    return kanji != nullptr;
}

std::uint64_t DictStore::entryCount() const {
    const std::shared_lock lock(mutex);
    return dictionary != nullptr ? dictionary->count() : 0;
}

std::uint64_t DictStore::kanjiCount() const {
    const std::shared_lock lock(mutex);
    return kanji != nullptr ? kanji->kanji_count() : 0;
}

std::vector<dict::SearchHit> DictStore::searchIndices(const std::string &query,
                                                      dict::SearchMode mode, std::uint32_t cap) {
    std::shared_ptr<dict::Dictionary> handle;
    {
        const std::shared_lock lock(mutex);
        handle = dictionary;
    }
    if (handle == nullptr) {
        return {};
    }
    return handle->search_indices(query, mode, cap);
}

std::vector<dict::SearchHit> DictStore::searchMeaningIndices(const std::string &query,
                                                             std::uint32_t cap) {
    std::shared_ptr<dict::Dictionary> handle;
    {
        const std::shared_lock lock(mutex);
        handle = dictionary;
    }
    if (handle == nullptr) {
        return {};
    }
    return handle->search_meaning_indices(query, cap);
}

std::optional<dict::DictEntry> DictStore::entryAt(std::uint32_t idx) {
    std::shared_ptr<dict::Dictionary> handle;
    {
        const std::shared_lock lock(mutex);
        handle = dictionary;
    }
    if (handle == nullptr) {
        return std::nullopt;
    }
    return handle->entry_at(idx);
}

std::vector<dict::DictEntry> DictStore::searchMiddle(const std::string &character,
                                                     std::uint32_t cap) {
    std::shared_ptr<dict::Dictionary> handle;
    {
        const std::shared_lock lock(mutex);
        handle = dictionary;
    }
    if (handle == nullptr) {
        return {};
    }
    return handle->search_middle(character, cap);
}

std::optional<dict::KanjiEntry> DictStore::lookupKanji(const std::string &character) {
    std::shared_ptr<dict::KanjiDictionary> handle;
    {
        const std::shared_lock lock(mutex);
        handle = kanji;
    }
    if (handle == nullptr) {
        return std::nullopt;
    }
    return handle->lookup(character);
}

std::vector<dict::KanjiStroke> DictStore::kanjiStrokes(const std::string &character) {
    std::shared_ptr<dict::KanjiDictionary> handle;
    {
        const std::shared_lock lock(mutex);
        handle = kanji;
    }
    if (handle == nullptr) {
        return {};
    }
    return handle->strokes(character);
}

std::vector<std::string> DictStore::kanjiContaining(const std::string &part) {
    std::shared_ptr<dict::KanjiDictionary> handle;
    {
        const std::shared_lock lock(mutex);
        handle = kanji;
    }
    if (handle == nullptr) {
        return {};
    }
    return handle->containing(part);
}

std::vector<Recognizer::Hit> DictStore::similarKanji(const QString &ch, std::uint32_t topK) {
    // Strokes from the kanji artifact (pen space already), matched
    // through the shared recognizer bank — one bank for the panel
    // matcher and the similar strip alike, no libdict copy.
    QVector<QVector<QPointF>> strokes;
    for (const dict::KanjiStroke &stroke : kanjiStrokes(ch.toStdString())) {
        QVector<QPointF> points;
        for (const dict::KanjiPoint &p : stroke.points) {
            points << QPointF(p.x, p.y);
        }
        strokes << points;
    }
    if (strokes.isEmpty()) {
        return {};
    }
    return Recognizer::shared().similar(strokes, ch, topK);
}
