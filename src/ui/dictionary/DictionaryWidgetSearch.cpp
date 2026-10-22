#include "ui/dictionary/DictionaryWidget.h"

#include "ui_DictionaryWidget.h"

#include "core/dictionary/DictionaryModel.h"
#include "core/state/SearchStore.h"

#include "ui/kanji/KanjiDialog.h"
#include "widgets/input/KanaInput.h"
#include "widgets/toolbutton/ToolButtonFit.h"

#include "dict.hpp"

#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <Qt>

void DictionaryWidget::submit(const QString &query) {
    // At the cursor, not the end: the field keeps focus while the
    // panel is open, so the cursor is where the user put it.
    // Refocus (the click moved it into the strip) to keep typing.
    ui->searchInput->insert(query);
    ui->searchInput->setFocus(Qt::OtherFocusReason);
    onSearch();
}

void DictionaryWidget::refreshReadings() {
    model->refreshDisplay();
    // The article renders from the model: re-render the live
    // selection (a no-op when hidden).
    if (ui->articleView->isVisible()) {
        onSelectionChanged();
    }
}

void DictionaryWidget::onSearch() {
    // Immediate path (Enter, candidate pick, reload): run the
    // store's current inputs now, bypassing the text debounce.
    SearchStore::Request request;
    request.text = SearchStore::shared().text();
    const SearchStore::Flags flags = SearchStore::shared().criteria();
    request.mode = flags.mode;
    request.meaning = flags.meaning;
    request.scope = SearchStore::shared().scope();
    request.trigger = SearchStore::Trigger::Text;
    if (executeRequest(request)) {
        lastImmediate = request.text;
    }
}

void DictionaryWidget::onRequest(const SearchStore::Request &request) {
    if (request.trigger == SearchStore::Trigger::Scope) {
        applyScopeRequest(request);
        return;
    }
    if (request.trigger == SearchStore::Trigger::Text && request.text == lastImmediate) {
        // Already ran through the immediate path (Enter): the old
        // code stopped the debounce timer there — drop the echo.
        return;
    }
    if (executeRequest(request)) {
        lastImmediate = request.text;
    }
}

bool DictionaryWidget::executeRequest(const SearchStore::Request &request) {
    // Romaji converts at the edge (typed, pasted, or candidate-);
    // kana passes through, so every input path searches converted.
    // Meaning mode takes the literal text (conversion disabled).
    const QString raw = request.text.trimmed();
    const QString query = request.meaning ? raw : KanaInput::searchText(raw);
    pendingQuery.clear();
    if (query.isEmpty()) {
        model->clear();
        return true;
    }
    if (kanaInput != nullptr && kanaInput->hasPending()) {
        // Mid-composition: no search. Batch conversion would
        // misread the pending tail (たn as たん); the settling
        // keystroke re-triggers through the debounce.
        // (Always settled in meaning mode — hasPending is false
        // with conversion disabled.)
        return false;
    }
    if (!model->isReady()) {
        // Loading: park the query and run it when the backend lands.
        pendingQuery = query;
        ui->statusLabel->setText(tr("Still loading dictionaries…"));
        return false;
    }
    if (request.meaning) {
        model->searchMeaning(query);
    } else {
        model->search(query, request.mode);
    }
    return true;
}

void DictionaryWidget::applySearchTarget() {
    // Radio pair, exactly one on — the store owns the target (dev
    // reloads rebuild the buttons, like the edge mode below).
    const bool meaning = SearchStore::shared().criteria().meaning;
    {
        const QSignalBlocker blockJ(ui->japaneseButton);
        const QSignalBlocker blockM(ui->meaningButton);
        ui->japaneseButton->setChecked(!meaning);
        ui->meaningButton->setChecked(meaning);
    }
    ui->prefixButton->setEnabled(!meaning);
    ui->suffixButton->setEnabled(!meaning);
    if (kanaInput != nullptr) {
        kanaInput->setConversionEnabled(!meaning);
    }
}

void DictionaryWidget::fitTargetButtons() {
    // Icon toggles: same square boxes as their siblings, so the
    // row never grows.
    const int side = toolButtonSide(ui->prefixButton, 40);
    fitWideToolButton(ui->japaneseButton, side);
    fitWideToolButton(ui->meaningButton, side);
}

void DictionaryWidget::applyScope() {
    // The store owns the scope (dev reloads rebuild the combo,
    // like the modes above).
    const QSignalBlocker block(ui->scopeCombo);
    ui->scopeCombo->setCurrentIndex(SearchStore::shared().scope());
}

QString DictionaryWidget::scopePath(int index) const {
    return index == 2 ? kokugoPath : index == 1 ? namesPath : wordsPath;
}

QString DictionaryWidget::scopeLabel(int index) const { return ui->scopeCombo->itemText(index); }

void DictionaryWidget::applyScopeRequest(const SearchStore::Request &request) {
    // Scope refines, it doesn't change the query language: the
    // file swaps, the live query re-searches once it lands. Only
    // valid scopes publish (the combo binding refuses missing
    // files), so the path below always resolves.
    const int index = request.scope;
    model->clear();
    if (kanjiDialog != nullptr) {
        kanjiDialog->reloadDictionary(scopePath(index));
    }
    ui->statusLabel->setText(tr("Loading %1…").arg(scopeLabel(index)));
    const QString raw = request.text.trimmed();
    pendingQuery = request.meaning ? raw : KanaInput::searchText(raw);
    // Empty parks nothing (rows already cleared); either way the
    // file swaps so the next query hits it.
    model->loadAsync(scopePath(index));
}

void DictionaryWidget::onSearchTargetChanged() {
    // Radio pair, exactly one on: the toggled button wins; an
    // off-toggle is refused (no dead mode with both dark).
    auto *toggled = qobject_cast<QPushButton *>(sender());
    if (toggled == nullptr) {
        return;
    }
    if (!toggled->isChecked()) {
        const QSignalBlocker block(toggled);
        toggled->setChecked(true);
        return;
    }
    const bool meaning = (toggled == ui->meaningButton);
    {
        const QSignalBlocker block(meaning ? ui->japaneseButton : ui->meaningButton);
        (meaning ? ui->japaneseButton : ui->meaningButton)->setChecked(false);
    }
    const SearchStore::Flags flags = SearchStore::shared().criteria();
    if (flags.meaning == meaning) {
        return;
    }
    SearchStore::shared().setFlags(flags.mode, meaning);
    applySearchTarget();
    // Queries don't carry across modes (romaji one way, English
    // the other): the emptying textChanged clears the rows through
    // the normal path.
    ui->searchInput->clear();
}

void DictionaryWidget::onDictionariesLoaded(bool ok) {
    if (!ok) {
        ui->statusLabel->setText(tr("Could not read the dictionary file."));
        return;
    }
    const SearchStore::Flags flags = SearchStore::shared().criteria();
    ui->statusLabel->setText(tr("%1 entries · %2")
                                 .arg(model->entryCount())
                                 .arg(scopeLabel(SearchStore::shared().scope())));
    if (!pendingQuery.isEmpty()) {
        if (flags.meaning) {
            model->searchMeaning(pendingQuery);
        } else {
            model->search(pendingQuery, flags.mode);
        }
        pendingQuery.clear();
    }
}

void DictionaryWidget::onSearchFinished(int hits, double searchMs) {
    ui->statusLabel->setText(tr("%1 hits · %2 ms").arg(hits).arg(searchMs, 0, 'f', 1));
}
