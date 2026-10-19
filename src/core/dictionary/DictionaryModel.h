#pragma once

#include "dict.hpp"

#include <QAbstractTableModel>
#include <QThread>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

class DictWorker;

/// One source-language attestation.
struct SourceDisplay {
    QString lang;
    QString text;
};

/// One sense shaped for display (zkanji-style runs): abbreviated
/// tag labels per category plus the joined gloss text. Senses
/// without English glosses are dropped upstream (no empty runs).
/// `*Tips` parallel their label lists one-to-one (built in the same
/// pass): full descriptions behind each abbreviated tag for native
/// hover tooltips.
struct SenseDisplay {
    QStringList pos;
    QStringList misc;
    QStringList field;
    QStringList posTips;
    QStringList miscTips;
    QStringList fieldTips;
    QString gloss;
    QStringList xref;
    QStringList ant;
    QStringList onlyKeb;
    QStringList onlyReb;
    QStringList notes;
    QVector<SourceDisplay> sources;
};

/// Abbreviated misc tag for "usually written using kana alone"
/// (`&uk;` → `uk`): painted warning-red wherever tags show, since
/// the kanji form alongside it is rarely used.
inline bool isUsuallyKana(const QString &tag) { return tag == QStringLiteral("uk"); }

/// One headword form with annotations.
struct ElementDisplay {
    QString form;
    QStringList info;
    QStringList priorities;
    bool noKanji = false;
    QStringList restrictedTo;
};

/// Full article for one row (the right pane renders this).
struct ArticleDisplay {
    QString heading;
    QString reading;
    quint32 entSeq = 0;
    /// JLPT level 1–5, 0 when unknown.
    quint8 jlpt = 0;
    QVector<ElementDisplay> kanji;
    QVector<ElementDisplay> readings;
    QVector<SenseDisplay> senses;
};
Q_DECLARE_METATYPE(QVector<SenseDisplay>)

/// Dictionary results model: three columns (Heading, Reading,
/// Meaning) with one row per index hit. Rows are created for all
/// returned indices at once (cheap); each row's contents come from
/// [`Dictionary::entry_at`] on first paint and are cached after.
/// Loading and index lookup run on a dedicated dict thread (see
/// `DictWorker`); this object only owns rows and the generation id.
/// Not ready until `loaded` fires — searches before that are
/// dropped.
class DictionaryModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit DictionaryModel(QObject *parent = nullptr);
    ~DictionaryModel() override;

    /// Load one dictionary artifact in the background; `loaded(ok)`
    /// when settled. Reloadable (scope switches swap the file).
    void loadAsync(const QString &path);
    /// Artifact path (for sibling models over the same file).
    QString dictPath() const { return dictFile; }
    /// Artifacts loaded and searchable.
    bool isReady() const { return ready; }
    /// Record count (0 until loaded).
    quint64 entryCount() const { return entries; }
    /// Prefix/suffix edge (both on: substring) search over both
    /// files (words first), replacing all rows; no-op until ready.
    /// Scope defaults to both files (the kanji dialog relies on it).
    void search(const QString &query, dict::SearchMode mode);
    /// English gloss substring search (same rows, same shape);
    /// no-op until ready.
    void searchMeaning(const QString &query);
    /// Dict thread for dialog workers (one thread for the whole
    /// dictionary UI — kanji loads run here, never on their own
    /// thread).
    QThread *workerThread() { return &dictThread; }
    /// Load worker itself (dialog kanji loads connect here).
    DictWorker *dictWorker() { return worker; }
    /// Drop all rows (empty query); always allowed.
    void clear();
    /// Full article for one row, converted fresh (never cached —
    /// selections are rare, scroll paints are not).
    ArticleDisplay articleFor(int row) const;
    /// Word entries for one kanji (dialog Words tab) via middle
    /// search, relevance-ranked, capped. Empty without a loaded
    /// backend. UI-thread call like `articleFor` (corrupt data
    /// logs, never unwinds).
    QVector<dict::DictEntry> wordsContaining(const QString &ch, uint cap) const;
    /// Re-read visible text under the live script setting (readings
    /// and kana headings only — tags and glosses are script-free).
    void refreshDisplay();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    /// Structured senses for the row (`Qt::UserRole` carries
    /// ent_seq). The delegate lays out zkanji-style runs from these
    /// (number, small tag runs, gloss); empty when the hit has no
    /// displayable sense.
    static constexpr int SensesRole = Qt::UserRole + 1;
    /// Deinflected (conjugated-form) hit: the delegate tints these
    /// rows so they read apart from direct matches.
    static constexpr int DeinflectedRole = Qt::UserRole + 2;
    /// Joined inflection names for a deinflected hit (`past`,
    /// `-tara conditional, past`, … — zkanji vocabulary, `, `
    /// separator like its results list); empty on direct hits. The
    /// delegate leads the meaning runs with it.
    static constexpr int InflectionRole = Qt::UserRole + 3;
    /// Highlight needle for heading/reading cells: the candidate
    /// surface on deinflected hits, the query itself on direct ones
    /// (empty when neither applies, e.g. cleared rows). The
    /// delegate inverts its first occurrence per cell.
    static constexpr int MatchRole = Qt::UserRole + 4;
    /// Kana-only row (first sense tagged `uk`, like zkanji's
    /// KanaOnly note): the delegate paints its heading at slight
    /// text transparency, so usually-kana words read apart from
    /// kanji ones.
    static constexpr int KanaOnlyRole = Qt::UserRole + 5;

signals:
    /// Load settled (ok = both artifacts decoded).
    void loaded(bool ok);
    /// Search rows replaced (hits = row count); for the status line.
    void searchFinished(int hits, double searchMs);
    /// Worker requests (dict thread).
    void requestLoad(const QString &path);
    void requestSearch(QString query, dict::SearchMode mode, quint32 cap, quint64 generation);
    void requestSearchMeaning(QString query, quint32 cap, quint64 generation);

private slots:
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void onLoaded(bool ok, quint64 total);
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void onSearched(std::vector<dict::SearchHit> hits, double searchMs, quint64 generation);

private:
    /// One converted row (filled on first paint, then cached).
    /// Readings stay raw kana: script conversion happens at read
    /// time (`data`), so a setting flip only repaints.
    struct CachedRow {
        QString heading;
        QString reading;
        QString meaning;
        QVector<SenseDisplay> senses;
        bool headingIsKana = false;
        quint32 entSeq = 0;
    };

    /// Row contents, converting through the model's own handle on
    /// first request (`entry_at` throws nothing across — misses and
    /// FFI failures both land as an empty row).
    const CachedRow &rowAt(int row) const;

    /// Render one entry's senses as "gloss; gloss | …" (flat text
    /// for accessibility; the delegate paints structured runs from
    /// [`SensesRole`]). Senses without glosses are skipped.
    static QString formatSenses(const dict::DictEntry &entry);
    /// Structured senses for [`SensesRole`]: per-sense abbreviated
    /// tag labels plus joined gloss text, gloss-less senses dropped.
    static QVector<SenseDisplay> displaySenses(const dict::DictEntry &entry);
    /// Display form for one row: the first form containing the
    /// query, else the entry's own first form. Rows matched through
    /// a secondary form (query 在る on the 有る entry) would
    /// otherwise show an apparently unrelated headword.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    static QString matchForm(const QStringList &forms, const QString &query,
                             const QString &fallback);

    /// Search cap: effectively uncapped — layout cost no longer
    /// depends on row count (only visible rows are ever measured,
    /// the rest interpolate at the minimum height), so any number
    /// of returned rows stays smooth.
    static constexpr std::uint32_t hitCap = 0xFFFFFFFF;

    /// Dict thread (owned here; see `workerThread`).

    QThread dictThread;
    DictWorker *worker = nullptr;
    QString dictFile;
    quint64 generation = 0;
    bool ready = false;
    quint64 entries = 0;
    std::vector<dict::SearchHit> rows;
    mutable std::vector<std::optional<CachedRow>> cache;
    /// Query behind the live rows: row display prefers the form
    /// containing it (see `matchForm`).
    QString lastQuery;
};
