#include "ui/kanji/KanjiDialog.h"

#include "core/dictionary/DictWorker.h"
#include "core/dictionary/DictionaryModel.h"
#include "core/dictionary/KanjiInfo.h"
#include "core/palette/MaterialPalette.h"
#include "core/recognizer/Recognizer.h"
#include "core/recognizer/RecognizerWorker.h"
#include "core/settings/MemorizedState.h"
#include "dev/UiForm.h"
#include "ui/kanji/KanjiStrokeView.h"
#include "ui_KanjiDialog.h"
#include "widgets/list/CandidateList.h"
#include "widgets/table/DictionaryTable.h"
#include "widgets/table/DictionaryTableDelegate.h"
#include "widgets/text/RunLabel.h"
#include "widgets/tooltip/KanjiHover.h"

#include "dict.hpp"

#include <QApplication>
#include <QChar>
#include <QCheckBox>
#include <QDialog>
#include <QEvent>
#include <QList>
#include <QListWidget>
#include <QMetaType>
#include <QObject>
#include <QPalette>
#include <QPushButton>
#include <QSlider>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QTableView>
#include <QTimer>
#include <QVector>
#include <Qt>
#include <QtGlobal>

#include <vector>

namespace {
/// Joined list value, `-` when the source carries none.
QString joinedOrDash(const QStringList &items, const QString &sep) {
    return items.isEmpty() ? QStringLiteral("-") : items.join(sep);
}
} // namespace

KanjiDialog::KanjiDialog(KanjiInfo *info, DictionaryModel *modelIn, QWidget *parent)
    : QDialog(parent), kanjiInfo(info), model(modelIn) {
    auto [built, form] = buildForm<Ui::KanjiDialog>(this, "ui/kanji/KanjiDialog.ui");
    ui = built;
    devForm = form;
    wireUi();
    // Window-level kanji hover (shared card for the words table
    // and the candidate strips).
    new KanjiHover(this, kanjiInfo);
}

void KanjiDialog::wireUi() {
    strokeView = ui->kanjiStrokeView;
    tabs = ui->kanjiTabs;
    infoLabel = ui->kanjiInfo;
    counter = ui->kanjiStrokeCounter;
    wordsTable = ui->kanjiWordsTable;
    similarCheck = ui->kanjiSimilarCheck;
    similarStrip = ui->kanjiSimilarStrip;
    partsCheck = ui->kanjiPartsCheck;
    partsStrip = ui->kanjiPartsStrip;
    partOfCheck = ui->kanjiPartOfCheck;
    partOfStrip = ui->kanjiPartOfStrip;
    // Playback icons are style-native (runtime only).
    const auto style = this->style();
    ui->kanjiRestartButton->setIcon(style->standardIcon(QStyle::SP_BrowserReload));
    ui->kanjiPlayButton->setIcon(style->standardIcon(QStyle::SP_MediaPlay));
    ui->kanjiPauseButton->setIcon(style->standardIcon(QStyle::SP_MediaPause));
    ui->kanjiStopButton->setIcon(style->standardIcon(QStyle::SP_MediaStop));
    ui->kanjiPrevButton->setIcon(style->standardIcon(QStyle::SP_MediaSkipBackward));
    ui->kanjiNextButton->setIcon(style->standardIcon(QStyle::SP_MediaSkipForward));
    connect(ui->kanjiRestartButton, &QPushButton::clicked, strokeView, &KanjiStrokeView::restart);
    connect(ui->kanjiPlayButton, &QPushButton::clicked, strokeView, &KanjiStrokeView::play);
    connect(ui->kanjiPauseButton, &QPushButton::clicked, strokeView, &KanjiStrokeView::pause);
    connect(ui->kanjiStopButton, &QPushButton::clicked, strokeView, &KanjiStrokeView::stop);
    connect(ui->kanjiPrevButton, &QPushButton::clicked, strokeView, &KanjiStrokeView::stepBack);
    connect(ui->kanjiNextButton, &QPushButton::clicked, strokeView, &KanjiStrokeView::stepForward);
    connect(ui->kanjiSpeed, &QSlider::valueChanged, strokeView, &KanjiStrokeView::setSpeed);
    strokeView->setSpeed(ui->kanjiSpeed->value());
    connect(strokeView, &KanjiStrokeView::progressChanged, this, &KanjiDialog::onStrokeProgress);
    // Words tab: the sibling model over the shared table (same
    // delegate as the main list).
    wordsModel = new DictionaryModel(wordsTable);
    wordsTable->setModel(wordsModel);
    wordsDelegate = new DictionaryTableDelegate(wordsTable);
    wordsTable->setItemDelegate(wordsDelegate);
    connect(wordsTable, &QTableView::clicked, this, &KanjiDialog::onWordClicked);
    if (model != nullptr) {
        wordsModel->loadAsync(model->dictPath());
        // Searches before the backend lands drop silently: resend
        // the current character once it is ready.
        connect(wordsModel, &DictionaryModel::loaded, this, &KanjiDialog::onWordsLoaded);
    }
    // Checkbox rows (headings in the form; strips fill on load).
    wireStripRow(similarCheck, similarStrip, MemorizedState::KanjiStrip::Similar);
    wireStripRow(partsCheck, partsStrip, MemorizedState::KanjiStrip::Parts);
    wireStripRow(partOfCheck, partOfStrip, MemorizedState::KanjiStrip::PartOf);
    // Kanji loads run on the model's worker (one thread for the
    // whole dictionary UI); stale generations drop on arrival.
    // The model outlives the dialog (member order in the widget).
    qRegisterMetaType<std::vector<Recognizer::Hit>>();
    if (model != nullptr && model->dictWorker() != nullptr) {
        auto *worker = model->dictWorker();
        connect(this, &KanjiDialog::requestLoad, worker, &DictWorker::loadParts);
        connect(this, &KanjiDialog::requestLoad, worker, &DictWorker::loadPartOf);
        connect(worker, &DictWorker::partsLoaded, this, &KanjiDialog::onPartsLoaded);
        connect(worker, &DictWorker::partOfLoaded, this, &KanjiDialog::onPartOfLoaded);
        // Similar matching has its own worker (same model thread).
        similarWorker = new RecognizerWorker;
        similarWorker->moveToThread(model->workerThread());
        connect(this, &KanjiDialog::requestLoad, similarWorker, &RecognizerWorker::loadSimilar);
        connect(similarWorker, &RecognizerWorker::similarLoaded, this,
                &KanjiDialog::onSimilarLoaded);
    }
}

KanjiDialog::~KanjiDialog() {
    // Workers die with their threads (both model-owned): queued
    // answers to a dead dialog drop on their own.
    if (similarWorker != nullptr) {
        similarWorker->deleteLater();
    }
}

void KanjiDialog::wireStripRow(QCheckBox *check, CandidateList *strip,
                               MemorizedState::KanjiStrip kind) {
    strip->refreshScale();
    // Restored open state (signals blocked: the dialog is not
    // shown yet, so no resize chase — first show lays out fresh).
    const bool open = MemorizedState::kanjiStripOpen(kind);
    check->blockSignals(true);
    check->setChecked(open);
    check->blockSignals(false);
    strip->setVisible(open);
    connect(check, &QCheckBox::toggled, this, [this, strip, kind](bool on) {
        MemorizedState::setKanjiStripOpen(kind, on);
        onStripToggled(strip, on);
    });
    connect(strip, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) { setCharacter(item->text()); });
}

void KanjiDialog::onStripToggled(CandidateList *strip, bool on) {
    // Baseline first: showing the strip relayouts synchronously,
    // so anything read after setVisible is already polluted.
    const int tabsHeight = tabs != nullptr ? tabs->height() : 0;
    strip->setVisible(on);
    // The main area keeps its size: the window grows/shrinks
    // instead of the tabs. Nested layouts settle over several
    // event-loop passes (the first brings nothing), so the chase
    // starts deferred and runs until the drift holds still
    // (bounded — settle depth is tiny).
    QTimer::singleShot(0, this,
                       [this, strip, tabsHeight] { compensateTabs(strip, tabsHeight, 0); });
}

void KanjiDialog::compensateTabs(CandidateList *strip, int tabsHeight, int round) {
    if (strip != nullptr) {
        strip->refreshGrid();
    }
    if (tabs == nullptr) {
        return;
    }
    // Every round measures and compensates; rounds run
    // unconditionally (an early round legitimately sees no drift
    // yet — only a settled round proves stability).
    constexpr int maxRounds = 3;
    if (const int drift = tabs->height() - tabsHeight; drift != 0) {
        resize(width(), height() - drift);
    }
    if (round < maxRounds) {
        QTimer::singleShot(0, this, [this, strip, tabsHeight, round] {
            compensateTabs(strip, tabsHeight, round + 1);
        });
    }
}

void KanjiDialog::reloadDictionary(const QString &path) {
    if (wordsModel != nullptr) {
        wordsModel->loadAsync(path);
    }
}

void KanjiDialog::setCharacter(const QString &ch) {
    if (kanjiInfo == nullptr) {
        return;
    }
    const KanjiInfo::Display card = kanjiInfo->display(ch);
    if (!card.valid) {
        return;
    }
    currentChar = ch;
    // Retargets land on Info (the Words tab belongs to the old
    // character until the user switches back).
    if (tabs != nullptr) {
        tabs->setCurrentIndex(0);
    }
    // Parts group reflects the new character immediately (data is
    // already in hand).
    const bool decomposable =
        !(card.parts.size() == 1 && card.parts.front() == ch) && !card.parts.isEmpty();
    partsCheck->setEnabled(decomposable);
    if (!decomposable) {
        partsCheck->setChecked(false);
    }
    // Words refill through the sibling model (Middle mode);
    // strips refill from the load worker (stale strip content
    // clears now so retargets never flash it; plain clears, never
    // the panel placeholder).
    if (wordsModel != nullptr) {
        wordsModel->search(ch, dict::SearchMode::kMiddle);
    }
    similarStrip->clear();
    partsStrip->clear();
    partOfStrip->clear();
    emit requestLoad(ch, ++loadGeneration);
    strokeView->setStrokes(kanjiInfo->strokes(ch));
    const QString radical =
        card.radical > 0 ? tr("%1 %2").arg(card.radical).arg(KanjiInfo::radicalGlyph(card.radical))
                         : QStringLiteral("-");
    setWindowTitle(
        tr("Kanji information - Stroke count: %1 Radical: %2").arg(card.strokes).arg(radical));
    // One rich-text block: caption lead run plus value run per
    // row, one paragraph each (the `\n` lead opens a new
    // paragraph, like the article meta lines). Any span selects
    // and copies natively through the label itself.
    QVector<RunLabel::Run> runs;
    bool first = true;
    const auto row = [&](const QString &caption, const QString &value) {
        const QString head =
            (first ? QString() : QStringLiteral("\n")) + caption + QStringLiteral(" ");
        runs << RunLabel::Run{head, RunLabel::Ink::Dim, true, true};
        runs << RunLabel::Run{value, RunLabel::Ink::Body};
        first = false;
    };
    row(tr("Meanings:"), card.meanings.join(QStringLiteral(", ")));
    row(tr("ON readings:"), card.on.join(QStringLiteral("\u3001")));
    row(tr("Kun readings:"), card.kun.join(QStringLiteral("\u3001")));
    row(tr("Nanori:"), joinedOrDash(card.nanori, QStringLiteral("\u3001")));
    row(tr("Chinese:"), joinedOrDash(card.chinese, QStringLiteral(", ")));
    row(tr("Korean:"), joinedOrDash(card.korean, QStringLiteral(", ")));
    row(tr("Vietnamese:"), joinedOrDash(card.vietnamese, QStringLiteral(", ")));
    const QString gradeText =
        card.grade > 0 ? tr("%1 %2").arg(KanjiInfo::gradeName(card.grade)).arg(card.grade)
                       : QStringLiteral("-");
    row(tr("Grade:"), gradeText);
    const QString jlptText = card.jlpt > 0 ? QString::number(card.jlpt) : QStringLiteral("-");
    row(tr("JLPT:"), jlptText);
    row(tr("Frequency:"), card.freq > 0 ? QString::number(card.freq) : QStringLiteral("-"));
    row(tr("Radical names:"), joinedOrDash(card.radNames, QStringLiteral(", ")));
    row(tr("Strokes:"), QString::number(card.strokes));
    row(tr("JIS X0208:"), card.jis.isEmpty() ? QStringLiteral("-") : card.jis);
    row(tr("JIS X0212:"), joinedOrDash(card.jis212, QStringLiteral(", ")));
    row(tr("JIS X0213:"), joinedOrDash(card.jis213, QStringLiteral(", ")));
    row(tr("JIS X0208 refs:"), joinedOrDash(card.jis208Refs, QStringLiteral(", ")));
    const QList<uint> codepoints = card.character.toUcs4();
    row(tr("Unicode:"), codepoints.isEmpty()
                            ? QStringLiteral("-")
                            : QStringLiteral("0x%1").arg(QString::number(codepoints.front(), 16)));
    QString skipText =
        card.skip.isEmpty() ? QStringLiteral("-") : card.skip.join(QStringLiteral(", "));
    if (card.skipMisgrade && !card.skip.isEmpty()) {
        skipText += tr(" (miscount)");
    }
    row(tr("SKIP:"), skipText);
    row(tr("Four corner:"), joinedOrDash(card.fourCorner, QStringLiteral(", ")));
    row(tr("De Roo:"), joinedOrDash(card.deroo, QStringLiteral(", ")));
    row(tr("SH:"), joinedOrDash(card.shDesc, QStringLiteral(", ")));
    row(tr("Grade:"), gradeText);
    row(tr("JLPT:"), jlptText);
    row(tr("Nelson:"), joinedOrDash(card.nelsonC, QStringLiteral(", ")));
    row(tr("NJECD:"), joinedOrDash(card.njecd, QStringLiteral(", ")));
    row(tr("O'Neill:"), joinedOrDash(card.oneill, QStringLiteral(", ")));
    row(tr("De Roo refs:"), joinedOrDash(card.derooVariant, QStringLiteral(", ")));
    row(tr("SH refs:"), joinedOrDash(card.shRefs, QStringLiteral(", ")));
    infoLabel->setRuns(runs);
    onStrokeProgress(strokeView->shownCount(), strokeView->strokeCount());
    refreshTheme();
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void KanjiDialog::onStrokeProgress(int shown, int total) {
    Q_UNUSED(total);
    if (counter != nullptr) {
        counter->setText(QStringLiteral("%1").arg(shown, 2, 10, QChar(u'0')));
    }
}

void KanjiDialog::changeEvent(QEvent *event) {
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ThemeChange) {
        refreshTheme();
    }
    QDialog::changeEvent(event);
}

void KanjiDialog::refreshTheme() {
    // Rule fills re-resolve from the live palette (no stylesheets:
    // they bake at polish time and go stale across theme
    // switches). Caption/value inks resolve per paint inside
    // RunLabel, so nothing else needs a nudge here.
    const MaterialPalette theme = MaterialPalette::fromPalette(QApplication::palette());
    for (QWidget *line : findChildren<QWidget *>(QStringLiteral("kanjiRule"))) {
        QPalette pal = line->palette();
        pal.setColor(QPalette::Window, theme.cardBorder());
        line->setPalette(pal);
    }
}

void KanjiDialog::onWordsLoaded(bool ok) {
    // Backend landed after searches already dropped: run the
    // current character once.
    if (ok && wordsModel != nullptr && !currentChar.isEmpty()) {
        wordsModel->search(currentChar, dict::SearchMode::kMiddle);
    }
}

void KanjiDialog::onWordClicked(const QModelIndex &index) {
    if (wordsModel == nullptr || !index.isValid()) {
        return;
    }
    const QString query = wordsModel->index(index.row(), 0).data(Qt::DisplayRole).toString();
    if (!query.isEmpty()) {
        emit wordActivated(query);
    }
}

void KanjiDialog::onSimilarLoaded(const std::vector<Recognizer::Hit> &hits, quint64 generation) {
    // Stale generation (retarget mid-flight): drop it.
    if (generation != loadGeneration) {
        return;
    }
    if (hits.empty()) {
        similarCheck->setChecked(false);
        return;
    }
    QVector<CandidateHit> cells;
    for (const Recognizer::Hit &hit : hits) {
        cells << CandidateHit{hit.text, static_cast<double>(hit.score)};
    }
    similarStrip->setHits(cells);
}

void KanjiDialog::onPartsLoaded(const QStringList &parts, quint64 generation) {
    if (generation != loadGeneration) {
        return;
    }
    if (parts.isEmpty()) {
        partsCheck->setChecked(false);
        return;
    }
    QVector<CandidateHit> cells;
    for (const QString &part : parts) {
        cells << CandidateHit{part, 0.0};
    }
    partsStrip->setHits(cells);
}

void KanjiDialog::onPartOfLoaded(const QStringList &partOf, quint64 generation) {
    if (generation != loadGeneration) {
        return;
    }
    if (partOf.isEmpty()) {
        partOfCheck->setChecked(false);
        return;
    }
    QVector<CandidateHit> cells;
    for (const QString &user : partOf) {
        cells << CandidateHit{user, 0.0};
    }
    partOfStrip->setHits(cells);
}
