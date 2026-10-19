#include "ui/dictionary/DictionaryWidget.h"

#include "ui_DictionaryWidget.h"

#include "core/dictionary/DictionaryModel.h"

#include "ui/kanji/KanjiDialog.h"
#include "widgets/input/KanaInput.h"
#include "widgets/toolbutton/ToolButtonFit.h"

#include "dict.hpp"

#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
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
    searchDebounce->stop();
    // Romaji converts at the edge (typed, pasted, or candidate-);
    // kana passes through, so every input path searches converted.
    // Meaning mode takes the literal text (conversion disabled).
    const QString raw = ui->searchInput->text().trimmed();
    const QString query = meaningMode ? raw : KanaInput::searchText(raw);
    pendingQuery.clear();
    if (query.isEmpty()) {
        model->clear();
        return;
    }
    if (kanaInput != nullptr && kanaInput->hasPending()) {
        // Mid-composition: no search. Batch conversion would
        // misread the pending tail (たn as たん); the settling
        // keystroke re-triggers through the debounce.
        // (Always settled in meaning mode — hasPending is false
        // with conversion disabled.)
        return;
    }
    if (!model->isReady()) {
        // Loading: park the query and run it when the backend lands.
        pendingQuery = query;
        ui->statusLabel->setText(tr("Still loading dictionaries…"));
        return;
    }
    if (meaningMode) {
        model->searchMeaning(query);
    } else {
        model->search(query, searchMode);
    }
}

void DictionaryWidget::onQueryChanged(const QString &text) {
    if (text.trimmed().isEmpty()) {
        searchDebounce->stop();
        onSearch();
        return;
    }
    searchDebounce->start();
}

void DictionaryWidget::onSearchModeChanged() {
    // Independent toggles: prefix-only, suffix-only, both on
    // (substring), or neither (exact forms only). Every
    // combination re-searches.
    const bool prefix = ui->prefixButton->isChecked();
    const bool suffix = ui->suffixButton->isChecked();
    const dict::SearchMode mode = prefix && suffix ? dict::SearchMode::kMiddle
                                  : suffix         ? dict::SearchMode::kSuffix
                                  : prefix         ? dict::SearchMode::kPrefix
                                                   : dict::SearchMode::kExact;
    if (searchMode == mode) {
        return;
    }
    searchMode = mode;
    onSearch();
}

void DictionaryWidget::applySearchTarget() {
    // Member owns the state (dev reloads rebuild the buttons, like
    // the edge mode above). Meaning mode is substring-only, so the
    // edge buttons go dark but keep their checks for the way back.
    {
        const QSignalBlocker blockJ(ui->japaneseButton);
        const QSignalBlocker blockM(ui->meaningButton);
        ui->japaneseButton->setChecked(!meaningMode);
        ui->meaningButton->setChecked(meaningMode);
    }
    ui->prefixButton->setEnabled(!meaningMode);
    ui->suffixButton->setEnabled(!meaningMode);
    if (kanaInput != nullptr) {
        kanaInput->setConversionEnabled(!meaningMode);
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
    // Member owns the state (dev reloads rebuild the combo, like
    // the modes above).
    const QSignalBlocker block(ui->scopeCombo);
    ui->scopeCombo->setCurrentIndex(scopeIndex);
}

QString DictionaryWidget::scopePath(int index) const {
    return index == 2 ? kokugoPath : index == 1 ? namesPath : wordsPath;
}

QString DictionaryWidget::scopeLabel(int index) const { return ui->scopeCombo->itemText(index); }

void DictionaryWidget::onScopeChanged(int index) {
    // Exactly one dictionary at a time: swap the loaded file, park
    // the live query, and re-search once it lands (the model
    // clears stale file-relative rows first). Never clears the
    // field — scope refines, it doesn't change the query language.
    if (index < 0 || index > 2 || index == scopeIndex) {
        return;
    }
    const QString path = scopePath(index);
    if (path.isEmpty()) {
        // Missing file: refuse (the option is disabled anyway).
        const QSignalBlocker block(ui->scopeCombo);
        ui->scopeCombo->setCurrentIndex(scopeIndex);
        return;
    }
    scopeIndex = index;
    model->clear();
    if (kanjiDialog != nullptr) {
        kanjiDialog->reloadDictionary(path);
    }
    ui->statusLabel->setText(tr("Loading %1…").arg(scopeLabel(index)));
    const QString raw = ui->searchInput->text().trimmed();
    pendingQuery = meaningMode ? raw : KanaInput::searchText(raw);
    // Empty parks nothing (rows already cleared); either way the
    // file swaps so the next query hits it.
    model->loadAsync(path);
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
    if (meaningMode == meaning) {
        return;
    }
    meaningMode = meaning;
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
    ui->statusLabel->setText(
        tr("%1 entries · %2").arg(model->entryCount()).arg(scopeLabel(scopeIndex)));
    if (!pendingQuery.isEmpty()) {
        if (meaningMode) {
            model->searchMeaning(pendingQuery);
        } else {
            model->search(pendingQuery, searchMode);
        }
        pendingQuery.clear();
    }
}

void DictionaryWidget::onSearchFinished(int hits, double searchMs) {
    ui->statusLabel->setText(tr("%1 hits · %2 ms").arg(hits).arg(searchMs, 0, 'f', 1));
}
