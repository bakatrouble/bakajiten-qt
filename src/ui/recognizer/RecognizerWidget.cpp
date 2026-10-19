#include "RecognizerWidget.h"

#include "ui_RecognizerWidget.h"

#include "core/data/DataPaths.h"
#include "core/dictionary/KanjiInfo.h"
#include "core/palette/MaterialPalette.h"
#include "core/recognizer/Recognizer.h"
#include "core/recognizer/RecognizerWorker.h"
#include "core/settings/MemorizedState.h"
#include "dev/UiForm.h"
#include "widgets/canvas/CanvasWidget.h"
#include "widgets/icons/ButtonIcon.h"
#include "widgets/list/CandidateList.h"
#include "widgets/toolbutton/ToolButtonFit.h"

#include <QByteArray>
#include <QCloseEvent>
#include <QDebug>
#include <QEvent>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QListWidget>
#include <QLoggingCategory>
#include <QMetaType>
#include <QObject>
#include <QPainter>
#include <QPointF>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSize>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <Qt>
#include <QtGlobal>

#include <vector>

Q_LOGGING_CATEGORY(uiDevLog, "ui.dev");

namespace {
/// Template-bank location: the data dir, with an explicit `MODEL`
/// override for dev runs (documented escape hatch, not a search).
QString templatePath() {
    if (const QByteArray env = qgetenv("MODEL"); !env.isEmpty()) {
        return QString::fromLocal8Bit(env);
    }
    return DataPaths::filePath(QStringLiteral("templates.bin"));
}
} // namespace

RecognizerWidget::RecognizerWidget(QWidget *parent) : QWidget(parent) {
    // Plain child widget on purpose: NO Qt::Window flag. A parented
    // native window still owns a shell surface, which Wayland
    // promotes to a top-level (observed: the panel escaped the main
    // window). A plain child is painted into the parent on every
    // backend — it cannot leave, needs no compositor, and the
    // client-drawn title bar above is its only chrome.
    auto [built, form] = buildForm<Ui::RecognizerWidget>(this, "ui/recognizer/RecognizerWidget.ui");
    ui = built;
    devForm = form;
    // Kanji hover cards (optional artifact — the card omits them
    // without it). Loaded from the data dir like the model bank.
    kanjiInfo = new KanjiInfo(this);
    kanjiInfo->load(DataPaths::filePath(QStringLiteral("kanji.bin")));
    wireUi();

    // Self-sizing: set memorized size if it exists. The first
    // show snaps it (live parent + layout then); stale garbage is
    // validated there (layout minimum, parent bounds,
    // square-canvas line).
    const QSize saved = MemorizedState::panelSize();
    if (saved.isValid()) {
        resize(saved.width(), saved.height());
    }

    // Elevation: soft shadow around the card. Follows the rounded
    // shape (unpainted corners stay transparent). Re-renders with
    // every child repaint — shrink the radius if stroke drawing ever
    // feels laggy under software rendering.
    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 4);
    shadow->setColor(MaterialPalette::scrim(0.7F));
    setGraphicsEffect(shadow);

    // Match thread owns the bank from here on: register the queued
    // payloads, wire requests one way and results back, then ask for
    // the first model on disk (fallbacks cascade on failure).
    qRegisterMetaType<QVector<QVector<QPointF>>>();
    qRegisterMetaType<std::vector<Recognizer::Hit>>();
    recognizerWorker = new RecognizerWorker;
    recognizerWorker->moveToThread(&matchThread);
    connect(this, &RecognizerWidget::requestLoadModel, recognizerWorker,
            &RecognizerWorker::loadModel);
    connect(this, &RecognizerWidget::requestRecognize, recognizerWorker,
            &RecognizerWorker::recognize);
    connect(recognizerWorker, &RecognizerWorker::modelLoaded, this,
            &RecognizerWidget::onModelLoaded);
    connect(recognizerWorker, &RecognizerWorker::recognized, this, &RecognizerWidget::onRecognized);
    matchThread.start();

    ui->statusLabel->setText(tr("Loading model…"));
    tryLoadModel();
    updateStatus();
}

/// Ask the worker for the data-dir bank when present; otherwise
/// settle on the missing-model text (the Dictionaries dialog
/// fetches it).
void RecognizerWidget::tryLoadModel() {
    const QString path = templatePath();
    if (QFile::exists(path)) {
        emit requestLoadModel(path);
        return;
    }
    modelReady = false;
    ui->statusLabel->setText(tr("No templates.bin yet — open Help > Dictionaries to download it."));
}

/// Reload bank + kanji info from the data dir (called after
/// dictionary downloads land).
void RecognizerWidget::reloadModel() {
    kanjiInfo->load(DataPaths::filePath(QStringLiteral("kanji.bin")));
    modelReady = false;
    templateCount = 0;
    tryLoadModel();
    updateStatus();
}

void RecognizerWidget::onModelLoaded(const QString &path, bool ok, quint64 templates) {
    if (!ok) {
        modelReady = false;
        ui->statusLabel->setText(tr("Could not read the handwriting model."));
        return;
    }
    templateCount = templates;
    modelReady = true;
    ui->statusLabel->setToolTip(path);
    updateStatus();
}

RecognizerWidget::~RecognizerWidget() {
    matchThread.quit();
    matchThread.wait();
    delete recognizerWorker;
    delete ui;
}

void RecognizerWidget::reloadUi() {
    // Toggle states live in the dying buttons: read first.
    const bool grid = ui->gridButton->isChecked();
    const bool kana = ui->kanaButton->isChecked();
    if (!reloadForm(this, devForm, ui, "ui/recognizer/RecognizerWidget.ui")) {
        return; // release no-op, or load failed (logged)
    }
    wireUi();
    // Restore without firing (would double-recognize via toggled);
    // canvas/grid application is visual only, kana needs one pass.
    {
        const QSignalBlocker blockGrid(ui->gridButton);
        const QSignalBlocker blockKana(ui->kanaButton);
        ui->gridButton->setChecked(grid);
        ui->kanaButton->setChecked(kana);
    }
    ui->canvas->setGridVisible(grid);
    onRecognize();
}

/// Install event filters, feed the canvas, and connect everything.
/// Runs once per form: initial build and every dev reload.
void RecognizerWidget::wireUi() {
    // Square chrome: sized to the interface font (fixed 40px boxes
    // would clip scaled text); refit on font changes below.
    fitToolButtons(ui->toolbar, chromeButton);
    if constexpr (kDevUiLoader) {
        // Dev aid: exact screen rects for click automation (see README).
        qCDebug(uiDevLog) << "kana"
                          << QRect(ui->kanaButton->mapToGlobal(QPoint(0, 0)),
                                   ui->kanaButton->size())
                          << "panel" << QRect(mapToGlobal(QPoint(0, 0)), size());
    }

    // Resize grip: plain corner zone with a diagonal cursor; sizing
    // is manual (clamped to the layout minimum and the parent rect)
    // because there is no window frame to grab.
    ui->resizeGrip->setCursor(Qt::SizeFDiagCursor);
    ui->resizeGrip->installEventFilter(this);

    // Candidates follow the interface font (size + row height);
    // square cells fit the strip. The list owns its cells, fades,
    // and scrolling; hover highlights and clicks fan out here.
    // Non-null by the loader contract (a missing .ui is fatal, see
    // dev/UiForm.h).
    // NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage)
    ui->candidatesList->refreshScale();
    connect(ui->candidatesList, &QListWidget::itemClicked, this,
            &RecognizerWidget::onCandidateClicked, Qt::UniqueConnection);

    ui->canvas->setStrokeData(&strokes, &open);
    connect(ui->canvas, &CanvasWidget::strokeStarted, this, &RecognizerWidget::onCanvasStarted);
    connect(ui->canvas, &CanvasWidget::strokeMoved, this, &RecognizerWidget::onCanvasMoved);
    connect(ui->canvas, &CanvasWidget::strokeEnded, this, &RecognizerWidget::onCanvasEnded);
    connect(ui->canvas, &CanvasWidget::clearDrawing, this, &RecognizerWidget::onClear);

    connect(ui->closeButton, &QPushButton::clicked, this, [this] { close(); });
    connect(ui->gridButton, &QPushButton::toggled, this, &RecognizerWidget::onGridToggled);
    connect(ui->kanaButton, &QPushButton::toggled, this, &RecognizerWidget::onKanaToggled);
    connect(ui->undoButton, &QPushButton::clicked, this, &RecognizerWidget::onUndo);
    connect(ui->redoButton, &QPushButton::clicked, this, &RecognizerWidget::onRedo);
    connect(ui->clearButton, &QPushButton::clicked, this, &RecognizerWidget::onClear);
    // Square toolbar icons (texts live in tooltips instead).
    ui->closeButton->setToolTip(tr("Close panel"));
    ButtonIcon::apply(ui->closeButton, QStringLiteral("close"));
    ui->gridButton->setToolTip(tr("Toggle grid"));
    ButtonIcon::apply(ui->gridButton, QStringLiteral("grid"));
    ui->kanaButton->setToolTip(tr("Kana matching"));
    ButtonIcon::apply(ui->kanaButton, QStringLiteral("kana-match"));
    ui->undoButton->setToolTip(tr("Undo stroke"));
    ButtonIcon::apply(ui->undoButton, QStringLiteral("undo"));
    ui->redoButton->setToolTip(tr("Redo stroke"));
    ButtonIcon::apply(ui->redoButton, QStringLiteral("redo"));
    ui->clearButton->setToolTip(tr("Clear canvas"));
    ButtonIcon::apply(ui->clearButton, QStringLiteral("erase"));
}

/// Snap a requested panel size so the canvas comes out square (see
/// `PanelSize.h`): the layout minimum floors it, the live chrome
/// inset picks the square-canvas line, the parent bounds cap it.
