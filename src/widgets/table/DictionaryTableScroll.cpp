#include "widgets/table/DictionaryTable.h"

#include "core/settings/MemorizedState.h"

#include <QScrollBar>

void DictionaryTable::resetScroll() {
    verticalScrollBar()->setValue(verticalScrollBar()->minimum());
    horizontalScrollBar()->setValue(horizontalScrollBar()->minimum());
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void DictionaryTable::onColumnResized(int logical, int oldSize, int newSize) {
    Q_UNUSED(oldSize);
    // Stretched meaning column excluded: its width is layout, not
    // a user choice.
    if (newSize > 0) {
        if (logical == 0) {
            MemorizedState::setResultsHeadingWidth(newSize);
        } else if (logical == 1) {
            MemorizedState::setResultsReadingWidth(newSize);
        }
    }
}
