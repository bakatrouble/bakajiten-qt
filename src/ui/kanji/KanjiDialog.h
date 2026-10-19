#pragma once

#include "core/recognizer/Recognizer.h"
#include "core/settings/MemorizedState.h"

#include <QDialog>
#include <QString>

#include <vector>

namespace Ui {
class KanjiDialog;
}

class QBoxLayout;

class CandidateList;
class DictionaryModel;
class DictionaryTableDelegate;
class KanjiInfo;
class KanjiStrokeView;
class QLabel;
class QPushButton;
class QCheckBox;
class QTabWidget;
class RecognizerWorker;
class DictionaryTable;
class RunLabel;

/// Kanji information dialog (see the zkanji mock): two tabs — Info
/// (S.O.D. stroke-order diagram plus readings/meanings and code
/// details) and Words (the main dictionary table over entries
/// containing the character) — plus Similar/Parts/Part-of checkbox
/// rows at the bottom (each heading toggles its candidate-list
/// strip; picking a character retargets the dialog).
/// Modeless (test- and workflow-friendly): the owner creates one
/// and drives it with `setCharacter`.
class KanjiDialog : public QDialog {
    Q_OBJECT
public:
    explicit KanjiDialog(KanjiInfo *info, DictionaryModel *model, QWidget *parent = nullptr);
    ~KanjiDialog() override;

    /// Show info for one character (no-op when uncovered — the
    /// caller filters double-clicks through `Display::valid`, this
    /// is the backstop).
    void setCharacter(const QString &ch);
    /// Scope switched while open: point the Words tab at the new
    /// file (the loaded signal resends the current character).
    void reloadDictionary(const QString &path);
    /// Character currently shown (for retarget assertions).
    QString character() const { return currentChar; }

signals:
    /// Word picked in the Words tab: the owner searches it (same
    /// path as article meta links).
    void wordActivated(const QString &query);
    /// Load request for the worker thread, echoing the generation
    /// (stale answers drop on arrival).
    void requestLoad(QString ch, quint64 generation);

protected:
    void changeEvent(QEvent *event) override;

private slots:
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void onStrokeProgress(int shown, int total);
    /// Similar shapes answered: fill the strip (stale
    /// generations drop; empty unchecks the heading).
    void onSimilarLoaded(const std::vector<Recognizer::Hit> &hits, quint64 generation);
    /// kradfile parts answered: same pattern.
    void onPartsLoaded(const QStringList &parts, quint64 generation);
    /// Part-of list answered: same pattern.
    void onPartOfLoaded(const QStringList &partOf, quint64 generation);
    /// Sibling words model ready: run the current character (its
    /// pre-ready searches dropped silently).
    void onWordsLoaded(bool ok);
    /// Words-table row clicked: search its heading.
    void onWordClicked(const QModelIndex &index);

private:
    /// Refresh code-rule fills on theme switches (run inks resolve
    /// per paint, so nothing else needs a nudge).
    void refreshTheme();
    /// Wire the form (runs once): member grabs, icons, models,
    /// worker, checkbox rows.
    void wireUi();
    /// One checkbox row (heading toggles the strip; clicking a cell
    /// retargets the dialog). Open state restores here and saves on
    /// every flip.
    void wireStripRow(QCheckBox *check, CandidateList *strip, MemorizedState::KanjiStrip kind);
    /// Checkbox flipped: show the strip and resize the window
    /// instead of the tab area.
    void onStripToggled(CandidateList *strip, bool on);
    /// Reclaim tab-area drift from a strip flip, chasing nested
    /// layout passes until stable (see onStripToggled).
    void compensateTabs(CandidateList *strip, int tabsHeight, int round);

    Ui::KanjiDialog *ui = nullptr;
    QWidget *devForm = nullptr;
    KanjiInfo *kanjiInfo = nullptr;
    DictionaryModel *model = nullptr;
    /// Character the dialog shows.
    QString currentChar;
    /// Load generation (retargets mid-flight answer stale).
    quint64 loadGeneration = 0;
    /// Similar-match worker, hosted on the model's dict thread
    /// (see `DictionaryModel::workerThread`).
    RecognizerWorker *similarWorker = nullptr;
    QTabWidget *tabs = nullptr;
    QWidget *wordsTab = nullptr;
    DictionaryTable *wordsTable = nullptr;
    /// Words list model (own artifacts + thread — same class as
    /// the main list, Middle mode over the dialog character).
    DictionaryModel *wordsModel = nullptr;
    DictionaryTableDelegate *wordsDelegate = nullptr;
    QCheckBox *similarCheck = nullptr;
    CandidateList *similarStrip = nullptr;
    QCheckBox *partsCheck = nullptr;
    CandidateList *partsStrip = nullptr;
    QCheckBox *partOfCheck = nullptr;
    CandidateList *partOfStrip = nullptr;
    KanjiStrokeView *strokeView = nullptr;
    /// The full kanjidic detail as one rich-text block (caption
    /// lead runs + value runs, one paragraph per row): any span
    /// selects and copies natively, captions included.
    RunLabel *infoLabel = nullptr;
    QLabel *counter = nullptr;
};
