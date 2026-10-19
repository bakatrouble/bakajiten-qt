#include "widgets/tooltip/KanjiTooltip.h"

#include "dev/UiForm.h"
#include "ui_KanjiTooltip.h"

#include "core/palette/MaterialPalette.h"
#include "widgets/tooltip/Tooltip.h"

#include "core/dictionary/KanjiInfo.h"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QEvent>
#include <QFont>
#include <QLabel>
#include <QLayout>
#include <QPalette>
#include <QSize>
#include <QString>
#include <QWidget>
#include <QtGlobal>

KanjiTooltip::KanjiTooltip(QWidget *parent) : Tooltip(parent) {
    // Card content comes from the .ui (same buildForm path as the
    // kanji dialog): setupUi builds the layout straight onto the
    // card, so object names resolve for tests and refresh alike.
    auto [built, form] = buildForm<Ui::KanjiTooltip>(card(), "widgets/tooltip/KanjiTooltip.ui");
    ui = built;
    devForm = form;
}

void KanjiTooltip::hover(const QString &ch, const KanjiInfo::Display &card) {
    Q_UNUSED(ch);
    pending = card;
    Tooltip::hover();
}

void KanjiTooltip::refresh() {
    const MaterialPalette theme = MaterialPalette::fromPalette(QApplication::palette());
    QFont glyphFont = font();
    glyphFont.setPointSizeF(qMax(12.0, glyphFont.pointSizeF() * 3.0));
    ui->kanjiGlyph->setFont(glyphFont);
    ui->kanjiGlyph->setText(pending.character);
    ui->kanjiStrokes->setText(QString::number(pending.strokes));
    ui->kanjiRadicalNumber->setText(pending.radical > 0 ? QString::number(pending.radical)
                                                        : QString());
    ui->kanjiRadical->setText(KanjiInfo::radicalGlyph(pending.radical));
    ui->kanjiMeanings->setText(pending.meanings.join(QStringLiteral(", ")));
    ui->kanjiMeanings->setVisible(!pending.meanings.isEmpty());
    ui->kanjiOn->setText(pending.on.join(QStringLiteral("、")));
    ui->kanjiOn->setVisible(!pending.on.isEmpty());
    ui->sectionOn->setVisible(!pending.on.isEmpty());
    ui->kanjiKun->setText(pending.kun.join(QStringLiteral("、")));
    ui->kanjiKun->setVisible(!pending.kun.isEmpty());
    ui->sectionKun->setVisible(!pending.kun.isEmpty());
    // Footer always shows: hyphens when the source carries no
    // grade/JLPT (bare KanjiVG-only records still get a card).
    ui->kanjiGrade->setText(
        pending.grade > 0 ? tr("%1: %2").arg(KanjiInfo::gradeName(pending.grade)).arg(pending.grade)
                          : tr("Grade: -"));
    ui->kanjiJlpt->setText(pending.jlpt > 0 ? QString::number(pending.jlpt) : QStringLiteral("-"));
    // Dim section labels and rule fills from the live palette
    // (no stylesheets: they bake at polish time and go stale
    // across theme switches).
    const QColor dim = theme.dimmed();
    for (QLabel *label :
         {ui->sectionStrokes, ui->sectionRadical, ui->sectionOn, ui->sectionKun, ui->sectionJlpt}) {
        QPalette pal = label->palette();
        pal.setColor(QPalette::WindowText, dim);
        label->setPalette(pal);
    }
    for (QWidget *line : {ui->ruleBottom}) {
        QPalette pal = line->palette();
        pal.setColor(QPalette::Window, theme.cardBorder());
        line->setPalette(pal);
    }
    // Re-resolve the layout now (not at show time): hidden labels
    // have no geometry, so a deferred pass would size from stale
    // widths and leave the height oversized. Invalidate first —
    // the spanning glyph's distributed minimums otherwise stick —
    // and flush the hide/show queue first: those post async
    // relayouts that invalidate+activate alone never sees, so the
    // window would measure (and minimum-pin) the stale tall
    // layout. Bounded: each pass settles strictly more.
    if (QLayout *layout = card()->layout()) {
        QSize hint(-1, -1);
        for (int i = 0; i < 4; ++i) {
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            layout->invalidate();
            layout->activate();
            const QSize next = card()->sizeHint();
            if (next == hint) {
                break;
            }
            hint = next;
        }
    }
}
