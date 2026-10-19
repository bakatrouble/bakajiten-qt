#include "ui/dictionary/DictionaryWidget.h"

#include "ui_DictionaryWidget.h"

#include "core/dictionary/KanjiInfo.h"
#include "ui/kanji/KanjiDialog.h"

#include <QString>

void DictionaryWidget::openKanjiInfo(const QString &ch) {
    // Emitters pass raw double-clicked characters (kana included):
    // only covered kanji get a dialog.
    if (kanjiInfo == nullptr || !kanjiInfo->display(ch).valid) {
        return;
    }
    if (kanjiDialog == nullptr) {
        kanjiDialog = new KanjiDialog(kanjiInfo, model, window());
        connect(kanjiDialog, &KanjiDialog::wordActivated, this,
                &DictionaryWidget::onArticleLinkActivated);
    }
    kanjiDialog->setCharacter(ch);
    kanjiDialog->show();
    kanjiDialog->raise();
    kanjiDialog->activateWindow();
}
