#include "widgets/table/DictionaryTable.h"

#include "core/dictionary/DictionaryModel.h"
#include "core/dictionary/MeaningBuilder.h"
#include "util/CellText.h"

#include <QFontMetrics>
#include <QPoint>
#include <QString>
#include <QTimer>
#include <QToolTip>
#include <QVector>

QString DictionaryTable::tagTipAt(const QPoint &pos) const {
    const int row = rowAt(pos.y());
    if (row < 0 || model() == nullptr || columnAt(pos.x()) != 2) {
        return {};
    }
    const QVector<SenseDisplay> senses =
        model()->index(row, 2).data(DictionaryModel::SensesRole).value<QVector<SenseDisplay>>();
    const QString inflection =
        model()->index(row, 2).data(DictionaryModel::InflectionRole).toString();
    const QVector<MeaningBuilder::Run> runs =
        MeaningBuilder::build(senses, font(), inflection, palette(), false);
    const QRect cellRect = visualRect(model()->index(row, 2));
    // TEMP-NEGATIVE-CONTROL: full-width walk.
    int x = CellText::textRect(cellRect).left();
    for (const MeaningBuilder::Run &run : runs) {
        const QFontMetrics metrics(run.font);
        const int w = metrics.horizontalAdvance(run.text);
        if (pos.x() >= x && pos.x() < x + w && !run.tip.isEmpty()) {
            return run.tip;
        }
        x += w;
        if (x > cellRect.right()) {
            break;
        }
    }
    return {};
}

void DictionaryTable::updateTagTip(const QPoint &pos) {
    const QString tip = (model() != nullptr) ? tagTipAt(pos) : QString();
    if (tip == tagTip) {
        // Same description (even across rows): running delay (or
        // visible tip) stands — re-arming per row would blink the
        // tip out on every sweep through a tag column.
        return;
    }
    hideTagTip();
    if (tip.isEmpty()) {
        return;
    }
    tagTip = tip;
    // Below the cursor: the tip must never cover the hovered run
    // (a tip under the cursor re-fires hover — the phantom-move
    // churn the kanji cards were built to avoid).
    tagAnchor = viewport()->mapToGlobal(pos) + QPoint(12, 20);
    tagTimer->start();
}

void DictionaryTable::hideTagTip() {
    tagTimer->stop();
    tagTip.clear();
    QToolTip::hideText();
}
