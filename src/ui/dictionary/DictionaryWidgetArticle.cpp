#include "ui/dictionary/DictionaryWidget.h"

#include "ui_DictionaryWidget.h"

#include "core/dictionary/DictionaryModel.h"
#include "widgets/text/ArticleView.h"

#include "core/settings/MemorizedState.h"

#include <QItemSelectionModel>
#include <QModelIndex>
#include <QObject>
#include <QSize>
#include <QSplitterHandle>
#include <QString>
#include <QToolButton>

void DictionaryWidget::onArticleClose() {
    // Same path as Escape: the pane hides with the selection.
    ui->resultsTable->clearSelections();
}

void DictionaryWidget::placeArticleClose() {
    if (articleClose == nullptr) {
        return;
    }
    const QSize viewport = ui->articleView->viewport()->size();
    articleClose->move(viewport.width() - articleClose->width() - 6, 6);
}

void DictionaryWidget::onSelectionChanged() {
    const QModelIndex current = ui->resultsTable->currentIndex();
    if (!current.isValid()) {
        ui->articleView->setVisible(false);
        return;
    }
    auto *model = qobject_cast<DictionaryModel *>(ui->resultsTable->model());
    if (model == nullptr) {
        ui->articleView->setVisible(false);
        return;
    }
    ui->articleView->setArticle(model->articleFor(current.row()));
    ui->articleView->setVisible(true);
    if (ui->articleView->width() == 0 && ui->mainSplit->width() > 0) {
        // First show: the splitter keeps the hidden zero width
        // instead of distributing — persisted width if it fits,
        // else the .ui 2:1 stretch. User drags after that are
        // never touched here (they persist on move).
        // setSizes distributes any shortfall/overflow
        // proportionally, so the entries must sum to exactly the
        // available space for the saved width to land on the
        // article verbatim — the table then takes the true
        // remainder. The handle takes no space here: it is still
        // hidden when the just-shown pane restores (layout is
        // pending), so children sum to total minus twice the
        // frame width (see splitterRoom).
        const int total = ui->mainSplit->width();
        const int saved = MemorizedState::articleWidth();
        const int articleWidth = (saved > 0 && saved < total) ? saved : total / 3;
        ui->mainSplit->setSizes({splitterRoom() - articleWidth, articleWidth});
    }
}

int DictionaryWidget::splitterRoom() const {
    int room = ui->mainSplit->width() - ui->mainSplit->frameWidth() * 2;
    const QSplitterHandle *handle = ui->mainSplit->handle(1);
    if (handle != nullptr && handle->isVisible()) {
        room -= ui->mainSplit->handleWidth();
    }
    return room;
}

void DictionaryWidget::onSplitMoved() {
    // Persist the article width only (the table takes the rest).
    // Never clobber with the hidden zero: the pane hides without
    // selection, so only live widths persist.
    if (ui->articleView->isVisible() && ui->articleView->width() > 0) {
        MemorizedState::setArticleWidth(ui->articleView->width());
    }
}

/// An article link was clicked: look up its headword (replaces the
/// query — links navigate, the recognizer button appends).
void DictionaryWidget::onArticleLinkActivated(const QString &query) {
    if (query.isEmpty()) {
        return;
    }
    // New context: the query replaces everything below.
    ui->searchInput->setText(query);
    onSearch();
}

/// First existing artifact on disk (`data/` under the build dir,
/// repo root, or above — same lookup as the recognizer model).
