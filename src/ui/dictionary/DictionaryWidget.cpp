#include "ui/dictionary/DictionaryWidget.h"

#include "ui_DictionaryWidget.h"

#include "core/data/DataPaths.h"
#include "core/dictionary/DictionaryModel.h"
#include "core/dictionary/KanjiInfo.h"
#include "dev/UiForm.h"
#include "ui/kanji/KanjiDialog.h"
#include "util/Romaji.h"
#include "widgets/icons/ButtonIcon.h"
#include "widgets/input/KanaInput.h"
#include "widgets/table/DictionaryTableDelegate.h"
#include "widgets/text/ArticleView.h"
#include "widgets/toolbutton/ToolButtonFit.h"

#include "dict.hpp"

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
#include <QTimer>
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
    searchDebounce = new QTimer(this);
    searchDebounce->setSingleShot(true);
    searchDebounce->setInterval(200);
    connect(searchDebounce, &QTimer::timeout, this, &DictionaryWidget::onSearch);
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
    // Mode button state survives dev reloads (the member owns
    // it, not the buttons). No autoExclusive: both on reads
    // substring, so exclusivity is manual in the slot.
    {
        const QSignalBlocker blockP(ui->prefixButton);
        const QSignalBlocker blockS(ui->suffixButton);
        ui->prefixButton->setChecked(searchMode != dict::SearchMode::kSuffix);
        ui->suffixButton->setChecked(searchMode != dict::SearchMode::kPrefix);
    }
    // Square toolbar icons (texts live in tooltips instead):
    // re-applied per form, re-tinted on theme switches below.
    ButtonIcon::apply(ui->suffixButton, QStringLiteral("suffix"));
    ButtonIcon::apply(ui->prefixButton, QStringLiteral("prefix"));
    ButtonIcon::apply(ui->recognizerButton, QStringLiteral("pen"));
    ButtonIcon::apply(ui->japaneseButton, QStringLiteral("kana"));
    ButtonIcon::apply(ui->meaningButton, QStringLiteral("latin"));
    connect(ui->searchInput, &QLineEdit::returnPressed, this, &DictionaryWidget::onSearch);
    connect(ui->searchInput, &QLineEdit::textChanged, this, &DictionaryWidget::onQueryChanged);
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
    connect(ui->prefixButton, &QPushButton::toggled, this, &DictionaryWidget::onSearchModeChanged);
    connect(ui->suffixButton, &QPushButton::toggled, this, &DictionaryWidget::onSearchModeChanged);
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
    connect(ui->scopeCombo, &QComboBox::currentIndexChanged, this,
            &DictionaryWidget::onScopeChanged);
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
    QString path = scopePath(scopeIndex);
    if (path.isEmpty()) {
        if (wordsPath.isEmpty()) {
            ui->statusLabel->setText(
                tr("No dictionaries yet — open Help > Dictionaries to download them."));
            return;
        }
        scopeIndex = 0;
        applyScope();
        path = wordsPath;
    }
    if (kanjiDialog != nullptr) {
        kanjiDialog->reloadDictionary(path);
    }
    ui->statusLabel->setText(tr("Loading %1…").arg(scopeLabel(scopeIndex)));
    const QString raw = ui->searchInput->text().trimmed();
    pendingQuery = meaningMode ? raw : KanaInput::searchText(raw);
    model->clear();
    model->loadAsync(path);
}
