#include "ui/dictionary/DictionaryWidget.h"

#include "ui_DictionaryWidget.h"

#include "core/data/DataPaths.h"
#include "core/dictionary/DictionaryModel.h"
#include "core/dictionary/KanjiInfo.h"
#include "core/state/SearchStore.h"
#include "core/state/SettingsStore.h"
#include "dev/UiForm.h"
#include "ui/kanji/KanjiDialog.h"
#include "util/Romaji.h"
#include "widgets/icons/ButtonIcon.h"
#include "widgets/input/KanaInput.h"
#include "widgets/table/DictionaryTableDelegate.h"
#include "widgets/text/ArticleView.h"
#include "widgets/toolbutton/ToolButtonFit.h"

#include "dict.hpp"

#include <rpp/operators/subscribe.hpp>
#include <rppqt/rppqt.hpp>

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardItemModel>
#include <QString>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>
#include <Qt>

namespace {
/// Enable/disable one combo option (missing dictionary files
/// darken instead of failing at switch time).
void setComboItemEnabled(QComboBox *combo, int index, bool enabled) {
    auto *model = qobject_cast<QStandardItemModel *>(combo->model());
    if (model == nullptr) {
        return;
    }
    if (QStandardItem *item = model->item(index); item != nullptr) {
        item->setEnabled(enabled);
    }
}
} // namespace

DictionaryWidget::DictionaryWidget(QWidget *parent) : QWidget(parent) {
    auto [built, form] = buildForm<Ui::DictionaryWidget>(this, "ui/dictionary/DictionaryWidget.ui");
    ui = built;
    devForm = form;
    // Model + delegate outlive the form (worker thread persists
    // across dev reloads); wireUi installs both per form.
    model = new DictionaryModel(this);
    delegate = new DictionaryTableDelegate(this);
    // Kanji hover cards (optional artifact — tooltips stay empty
    // without it, nothing else depends on it).
    kanjiInfo = new KanjiInfo(this);
    // Search inputs live in the store singleton: reset them so
    // every window (and every test window in-process) starts from
    // the defaults, then execute its processed requests.
    SearchStore::shared().setText(QString());
    SearchStore::shared().setFlags(dict::SearchMode::kPrefix, false);
    SearchStore::shared().setScope(0);
    SearchStore::shared().subscribe(
        this, [this](const SearchStore::Request &request) { onRequest(request); });
    // Live reading-script switch: the store publishes settings
    // accepts, so the MainWindow accept path only assigns.
    SettingsStore::shared().subscribe(this, [this](const AppSettings &) { refreshReadings(); });
    wireUi();

    connect(model, &DictionaryModel::loaded, this, &DictionaryWidget::onDictionariesLoaded);
    connect(model, &DictionaryModel::searchFinished, this, &DictionaryWidget::onSearchFinished);
    // Exactly one dictionary at a time (English/JMdict default).
    refreshArtifacts();
}

DictionaryWidget::~DictionaryWidget() { delete ui; }

/// Connect the single search signal. Runs once per form: initial
/// build and every dev reload.
void DictionaryWidget::wireUi() {
    // Non-null by the loader contract (a missing .ui is fatal, see
    // dev/UiForm.h).
    // NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage)
    ui->resultsTable->setModel(model);
    // Table config (stretch, uniform rows, hover, glide) lives in
    // `DictionaryTable` itself; scroll modes stay declarative in the
    // .ui. Only model + delegate re-install per form here.
    ui->resultsTable->setItemDelegate(delegate);
    // Mode button state syncs from the store (dev reloads
    // rebuild the buttons). No autoExclusive: both on reads
    // substring, so exclusivity is manual in the sync below.
    {
        const QSignalBlocker blockP(ui->prefixButton);
        const QSignalBlocker blockS(ui->suffixButton);
        const dict::SearchMode mode = SearchStore::shared().criteria().mode;
        ui->prefixButton->setChecked(mode != dict::SearchMode::kSuffix);
        ui->suffixButton->setChecked(mode != dict::SearchMode::kPrefix);
    }
    // Square toolbar icons (texts live in tooltips instead):
    // re-applied per form, re-tinted on theme switches below.
    ButtonIcon::apply(ui->suffixButton, QStringLiteral("suffix"));
    ButtonIcon::apply(ui->prefixButton, QStringLiteral("prefix"));
    ButtonIcon::apply(ui->recognizerButton, QStringLiteral("pen"));
    ButtonIcon::apply(ui->japaneseButton, QStringLiteral("kana"));
    ButtonIcon::apply(ui->meaningButton, QStringLiteral("latin"));
    // Search inputs publish into the store (rppqt signal
    // sources); the store's processed stream comes back through
    // onRequest. Sender-death breaks these bindings, so dev
    // reloads only ever feed the live form.
    rppqt::source::from_signal(*ui->searchInput, &QLineEdit::textChanged) |
        rpp::operators::subscribe([](const QString &text) { SearchStore::shared().setText(text); });
    connect(ui->searchInput, &QLineEdit::returnPressed, this, &DictionaryWidget::onSearch);
    // Hero field: fixed 16pt in the fixed 40px row (the app font
    // would look lost at 9pt). Derived from the application font so
    // the fallback chain (e.g. embedded Noto) survives the resize.
    {
        QFont inputFont = QApplication::font();
        inputFont.setPointSize(16);
        ui->searchInput->setFont(inputFont);
    }
    // IME-like romaji input (raw buffer behind the field; survives
    // dev reloads via rebind, like the model outlives the form).
    if (kanaInput == nullptr) {
        kanaInput = new KanaInput(ui->searchInput);
    } else {
        kanaInput->rebind(ui->searchInput);
    }
    // Independent toggles: prefix-only, suffix-only, both on
    // (substring), or neither (exact forms). Every combination is
    // a mode; the store re-searches on change.
    auto syncMode = [this] {
        const bool prefix = ui->prefixButton->isChecked();
        const bool suffix = ui->suffixButton->isChecked();
        const dict::SearchMode mode = prefix && suffix ? dict::SearchMode::kMiddle
                                      : suffix         ? dict::SearchMode::kSuffix
                                      : prefix         ? dict::SearchMode::kPrefix
                                                       : dict::SearchMode::kExact;
        SearchStore::shared().setFlags(mode, SearchStore::shared().criteria().meaning);
    };
    rppqt::source::from_signal(*ui->prefixButton, &QPushButton::toggled) |
        rpp::operators::subscribe([syncMode](bool) { syncMode(); });
    rppqt::source::from_signal(*ui->suffixButton, &QPushButton::toggled) |
        rpp::operators::subscribe([syncMode](bool) { syncMode(); });
    connect(ui->japaneseButton, &QPushButton::toggled, this,
            &DictionaryWidget::onSearchTargetChanged);
    connect(ui->meaningButton, &QPushButton::toggled, this,
            &DictionaryWidget::onSearchTargetChanged);
    applySearchTarget();
    // Scope combo: per-item file tooltips (re-applied per form),
    // index synced from the member below.
    ui->scopeCombo->setItemData(0, tr("Search both dictionaries"), Qt::ToolTipRole);
    ui->scopeCombo->setItemData(1, tr("JMdict words only"), Qt::ToolTipRole);
    ui->scopeCombo->setItemData(2, tr("JMnedict names only"), Qt::ToolTipRole);
    // Exactly one dictionary at a time: missing files refuse at
    // the edge (the option is disabled anyway), valid switches
    // publish — the store's scope stream swaps the file.
    rppqt::source::from_signal(*ui->scopeCombo, qOverload<int>(&QComboBox::currentIndexChanged)) |
        rpp::operators::subscribe([this](int index) {
            if (index < 0 || index > 2 || scopePath(index).isEmpty()) {
                const QSignalBlocker block(ui->scopeCombo);
                ui->scopeCombo->setCurrentIndex(SearchStore::shared().scope());
                return;
            }
            SearchStore::shared().setScope(index);
        });
    applyScope();
    connect(ui->recognizerButton, &QPushButton::toggled, this, &DictionaryWidget::toggleRecognizer);
    // Toolbar buttons stay square at any interface font (fixed .ui
    // boxes would clip scaled text): refit on font changes too.
    // All five carry icons now — the 40px floor rules, row height
    // unchanged.
    fitToolButtons(ui->searchRow_2, 40);
    fitTargetButtons();
    // Article pane follows row selection (fresh selection model per
    // setModel, so this re-runs per form). Starts hidden: no row is
    // selected on a fresh result set.
    ui->articleView->setVisible(false);
    ui->articleView->viewport()->installEventFilter(this);
    // Dismiss chip over the article's top-right (viewport child, so
    // it never moves with the text; visibility follows the pane).
    articleClose = new QToolButton(ui->articleView->viewport());
    articleClose->setObjectName(QStringLiteral("articleCloseButton"));
    articleClose->setText(QStringLiteral("×"));
    articleClose->setToolTip(tr("Close article"));
    articleClose->setAutoRaise(true);
    articleClose->setFixedSize(24, 24);
    articleClose->setCursor(Qt::ArrowCursor);
    connect(articleClose, &QToolButton::clicked, this, &DictionaryWidget::onArticleClose);
    placeArticleClose();
    // Meta links navigate (emitted by the run labels with the raw
    // headword query).
    connect(ui->articleView, &ArticleView::linkActivated, this,
            &DictionaryWidget::onArticleLinkActivated);
    // Splitter drags persist the article width (restored on the
    // next first show; the hidden zero never persists).
    connect(ui->mainSplit, &QSplitter::splitterMoved, this, &DictionaryWidget::onSplitMoved);
    // Window resizes flex the table and pin the article (stretch
    // 1/0): drags and the first-show restore still set explicit
    // sizes. (The .ui horstretch sits on the inner table, alone in
    // its own layout — the splitter never saw it.)
    ui->mainSplit->setStretchFactor(0, 1);
    ui->mainSplit->setStretchFactor(1, 0);
    // Floor for the flex: the article only yields once the table
    // cannot absorb more shrink, so tiny windows stay usable (and
    // the article scrolls) instead of crushing the table.
    ui->resultsFrame->setMinimumWidth(320);
    // I-beam over article text and gaps.
    ui->articleView->setCursor(Qt::IBeamCursor);
    ui->articleView->viewport()->setCursor(Qt::IBeamCursor);
    connect(ui->resultsTable->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &DictionaryWidget::onSelectionChanged);
    // Kanji dialog (double-click anywhere in a cell or the
    // article; the opener filters kana through kanji info).
    connect(ui->resultsTable, &DictionaryTable::kanjiDoubleClicked, this,
            &DictionaryWidget::openKanjiInfo);
    connect(ui->articleView, &ArticleView::kanjiDoubleClicked, this,
            &DictionaryWidget::openKanjiInfo);
}

bool DictionaryWidget::isOnRecognizerButton(const QPoint &global) const {
    // Local point against the local rect (geometry() lives in the
    // parent's frame — comparing it to a mapped point always missed).
    return ui->recognizerButton->rect().contains(ui->recognizerButton->mapFromGlobal(global));
}

bool DictionaryWidget::isOnSearchInput(const QPoint &global) const {
    return ui->searchInput->rect().contains(ui->searchInput->mapFromGlobal(global));
}

void DictionaryWidget::focusSearchInput() { ui->searchInput->setFocus(Qt::OtherFocusReason); }

QLineEdit *DictionaryWidget::searchField() { return ui->searchInput; }

QPoint DictionaryWidget::recognizerButtonAnchor() const {
    return ui->recognizerButton->mapTo(this, QPoint(0, ui->recognizerButton->height()));
}

void DictionaryWidget::setRecognizerVisible(const bool visible) const {
    const QSignalBlocker block(ui->recognizerButton);
    ui->recognizerButton->setChecked(visible);
}

void DictionaryWidget::changeEvent(QEvent *event) {
    // Theme switches repaint from the live palette (runs resolve
    // pens per paint; badges re-apply stylesheets) — no rebuild,
    // so the scroll position survives.
    if ((event->type() == QEvent::PaletteChange || event->type() == QEvent::ThemeChange) &&
        ui->articleView->isVisible()) {
        ui->articleView->refreshTheme();
    }
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ThemeChange) {
        ButtonIcon::refresh(this);
    }
    if (event->type() == QEvent::FontChange) {
        fitToolButtons(ui->searchRow_2, 40);
        fitTargetButtons();
    }
    QWidget::changeEvent(event);
}

bool DictionaryWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == ui->articleView->viewport()) {
        if (event->type() == QEvent::Resize) {
            placeArticleClose();
        }
    }
    return QWidget::eventFilter(watched, event);
}

/// Kanji hover reported by the article labels (hit-testing lives in
/// the labels; this slot only drives the tooltip card).
void DictionaryWidget::reloadUi() {
    const QString text = ui->searchInput->text();
    if (!reloadForm(this, devForm, ui, "ui/dictionary/DictionaryWidget.ui")) {
        return; // release no-op, or load failed (logged)
    }
    wireUi();
    if (!text.isEmpty()) {
        ui->searchInput->setText(text);
        onSearch();
    }
}

namespace {
QString existingPath(const QString &file) {
    const QString path = DataPaths::filePath(file);
    return QFile::exists(path) ? path : QString();
}
} // namespace

void DictionaryWidget::refreshArtifacts() {
    // Re-resolve every artifact (first run and post-download
    // alike): missing files disable their scope option instead of
    // failing, and the current scope reloads from its new path.
    kanjiInfo->load(DataPaths::filePath(QStringLiteral("kanji.bin")));
    wordsPath = existingPath(QStringLiteral("jmdict.bin"));
    namesPath = existingPath(QStringLiteral("jmnedict.bin"));
    kokugoPath = existingPath(QStringLiteral("kokugo.bin"));
    setComboItemEnabled(ui->scopeCombo, 1, !namesPath.isEmpty());
    setComboItemEnabled(ui->scopeCombo, 2, !kokugoPath.isEmpty());
    QString path = scopePath(SearchStore::shared().scope());
    if (path.isEmpty()) {
        if (wordsPath.isEmpty()) {
            ui->statusLabel->setText(
                tr("No dictionaries yet — open Help > Dictionaries to download them."));
            return;
        }
        SearchStore::shared().setScope(0);
        applyScope();
        path = wordsPath;
    }
    if (kanjiDialog != nullptr) {
        kanjiDialog->reloadDictionary(path);
    }
    ui->statusLabel->setText(tr("Loading %1…").arg(scopeLabel(SearchStore::shared().scope())));
    const QString raw = ui->searchInput->text().trimmed();
    pendingQuery = SearchStore::shared().criteria().meaning ? raw : KanaInput::searchText(raw);
    model->clear();
    model->loadAsync(path);
}
