#pragma once

#include "dict.hpp"

#include <QVector>
#include <QWidget>

namespace Ui {
class DictionaryWidget;
}
class DictionaryModel;
class DictionaryTableDelegate;
class KanaInput;
class KanjiDialog;
class KanjiInfo;
class KanjiTooltip;
class QLineEdit;
class QTimer;
class QToolButton;
struct ArticleDisplay;
struct ElementDisplay;
struct SenseDisplay;

/// Dictionary view: the main window's central widget, backed by the
/// `libdict` artifacts (`DictionaryModel` over the UniFFI bindings).
/// Layout comes from `DictionaryWidget.ui` — compiled uic in release,
/// QUiLoader in dev builds (same `Ui::` struct either way, see
/// `dev/UiForm.h`). Japanese text renders through the system
/// fontconfig fonts, so no bundled-font work is needed here.
class DictionaryWidget : public QWidget {
    Q_OBJECT
public:
    explicit DictionaryWidget(QWidget *parent = nullptr);
    ~DictionaryWidget() override;

    /// Drop the form and rebuild it from the .ui file on disk,
    /// keeping the current query (re-searched after rebuild).
    /// Dev builds only; no-op otherwise.
    void reloadUi();

    /// Whether a global point hits the handwriting open button.
    bool isOnRecognizerButton(const QPoint &global) const;
    /// Whether a global point hits the search field (clicks there
    /// only move the cursor while the panel is open).
    bool isOnSearchInput(const QPoint &global) const;
    /// Focus for panel-open: the query field keeps keyboard focus so
    /// typing continues while drawing; candidates insert at its
    /// cursor.
    void focusSearchInput();
    /// The query field itself (window-wide type-to-search
    /// redirection targets it).
    QLineEdit *searchField();
    /// Bottom-left of the open button, in this widget's coords
    /// (this is the panel's parent: the panel's top-left goes here).
    QPoint recognizerButtonAnchor() const;
    /// Kanji lookup for hover cards (the main window's shared
    /// KanjiHover reads through here; stable for the widget's life).
    KanjiInfo *kanjiData() const { return kanjiInfo; }
    /// Candidate pick from the panel: insert at the query cursor
    /// and search.
    void submit(const QString &query);
    /// Double-clicked kanji anywhere (table, article, candidates):
    /// show the information dialog (silently ignores kana — the
    /// emitters pass raw characters).
    void openKanjiInfo(const QString &ch);

    /// Re-resolve artifacts from the data dir and reload the
    /// current scope (called on first run and after dictionary
    /// downloads land).
    void refreshArtifacts();

    /// Re-render readings under the current script setting (called
    /// live when the setting changes — no research needed).
    void refreshReadings();

    void setRecognizerVisible(const bool visible) const;

protected:
    /// Article-pane hover: kanji cards under the cursor.
    bool eventFilter(QObject *watched, QEvent *event) override;
    /// Theme switch: baked article hexes outlive the palette they
    /// were rendered from, so re-render the live selection (stale
    /// dark text on a light pane is unreadable, and vice versa).
    /// PaletteChange fans out to every widget (ApplicationPalette-
    /// Change only reaches top-levels); ThemeChange covers platform
    /// switches that repolish without touching palettes.
    void changeEvent(QEvent *event) override;

signals:
    /// The handwriting button was toggled (checked = show panel).
    void toggleRecognizer(bool checked);

private slots:
    void onSearch();
    /// Query text changed: restart the debounce timer (clears
    /// immediately when emptied).
    void onQueryChanged(const QString &text);
    /// Prefix/suffix mode toggled: every combination is a mode
    /// (both on: substring, neither: exact). Re-searches on change.
    void onSearchModeChanged();
    /// Japanese/meaning target toggled: radio pair, exactly one on.
    /// Meaning mode disables the edge buttons and kana conversion;
    /// any switch clears the field (queries don't carry across).
    void onSearchTargetChanged();
    /// Dictionary scope changed (English/Names/Kokugo): swaps the
    /// loaded file and re-searches the live query (never clears —
    /// scope refines, it doesn't change the query language).
    void onScopeChanged(int index);
    /// Row selection moved: show and render the article pane, or
    /// hide it when nothing is selected.
    void onSelectionChanged();
    /// Splitter dragged: persist the article width (restored on the
    /// next first show).
    void onSplitMoved();
    /// Article close button: drop the row selection (same path as
    /// Escape — the pane hides with the selection).
    void onArticleClose();
    /// An article meta link was clicked: search its headword.
    void onArticleLinkActivated(const QString &query);
    /// Artifacts settled: show counts or the missing-file text.
    void onDictionariesLoaded(bool ok);
    /// Rows replaced: show hit count and time.
    void onSearchFinished(int hits, double searchMs);

private:
    /// Connect the single search signal. Runs once per form: initial
    /// build and every dev reload.
    void wireUi();
    /// Sync the target buttons, edge-button enables, and kana
    /// conversion from `meaningMode`. The member owns the state
    /// (dev reloads rebuild the buttons); runs in wireUi and on
    /// every target toggle.
    void applySearchTarget();
    /// Size the target toggles: text width fits, height matches
    /// the square siblings. Runs after every fitToolButtons.
    void fitTargetButtons();
    /// Sync the scope combo from `scopeIndex`. The member owns the
    /// state (dev reloads rebuild the combo); runs in wireUi.
    void applyScope();
    /// Artifact path for a scope index (empty when that file is
    /// missing — its combo option is disabled then).
    QString scopePath(int index) const;
    /// Scope label for the status line (combo item text).
    QString scopeLabel(int index) const;
    /// Pin the dismiss chip to the article viewport's top-right.
    void placeArticleClose();
    /// Child space in the splitter (frame always; the handle only
    /// while shown — it lags the just-shown pane, when it overlays
    /// instead of taking space).
    int splitterRoom() const;

    Ui::DictionaryWidget *ui = nullptr;
    /// Loader-built form (dev only; null with compiled uic).
    QWidget *devForm = nullptr;
    /// Search edge mode (prefix/suffix buttons; both on reads
    /// substring).
    dict::SearchMode searchMode = dict::SearchMode::kPrefix;
    /// Meaning (English gloss) search instead of Japanese. Owns the
    /// target buttons' state across dev reloads, like searchMode.
    bool meaningMode = false;
    /// Dictionary scope: index into the combo (0 English/JMdict,
    /// 1 Names/JMnedict, 2 Kokugo). Exactly one dictionary is ever
    /// loaded; switching swaps the file. Owns the combo state
    /// across dev reloads, like the modes above.
    int scopeIndex = 0;
    /// Resolved artifact paths (empty when the file is missing;
    /// the combo option is disabled then).
    QString wordsPath;
    QString namesPath;
    QString kokugoPath;
    /// Query typed while artifacts load: run once ready.
    QString pendingQuery;
    /// Results model (owns its dict thread; outlives the form across
    /// dev reloads).
    DictionaryModel *model = nullptr;
    /// Results-cell styling (installed per form; outlives reloads).
    DictionaryTableDelegate *delegate = nullptr;
    /// IME romaji buffer behind the search field (rebound per form).
    KanaInput *kanaInput = nullptr;
    /// Kanji hover info (optional `kanji.bin`; outlives the form).
    KanjiInfo *kanjiInfo = nullptr;
    /// Kanji information dialog (double-click anywhere; lazy).
    KanjiDialog *kanjiDialog = nullptr;
    /// Article dismiss chip (overlay child of the article viewport;
    /// rebuilt per form like the rest of `ui`).
    QToolButton *articleClose = nullptr;
    /// Live-search debounce: restarted per keystroke, fires
    /// `onSearch` (Enter bypasses it).
    QTimer *searchDebounce = nullptr;
};
