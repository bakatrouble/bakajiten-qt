#pragma once

#include "core/recognizer/RecognizerWorker.h"

#include <QElapsedTimer>
#include <QPointF>
#include <QThread>
#include <QVector>
#include <QWidget>

#include <cstdint>

namespace Ui {
class RecognizerWidget;
}
class KanjiInfo;
class QShowEvent;
class QListWidgetItem;

/// Handwriting recognition panel: canvas + right toolbar on top,
/// scrollable candidate strip at the bottom. Layout comes from
/// `RecognizerWidget.ui` — compiled uic in release, QUiLoader in dev
/// builds (same `Ui::` struct either way, see `dev/UiForm.h`).
/// Strokes live here (for painting and matching alike) and are
/// passed per query to the match worker (template bank on a QThread);
/// undo/redo are plain vector moves.
class RecognizerWidget : public QWidget {
    Q_OBJECT
public:
    explicit RecognizerWidget(QWidget *parent = nullptr);
    ~RecognizerWidget() override;

    /// Reload the bank + kanji info from the data dir (called
    /// after dictionary downloads land).
    void reloadModel();

    /// Drop the form and rebuild it from the .ui file on disk.
    /// Dev builds only; no-op otherwise. Toggle states are kept;
    /// strokes, model, and geometry survive (only the form is swapped).
    void reloadUi();

    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    /// Candidate picked (click, not drag): MainWindow inserts it
    /// into the search field.
    void candidateAccepted(const QString &text);
    /// Same candidate double-clicked fast: no second insert — the
    /// opener shows the kanji dialog instead.
    void candidateDoubleClicked(const QString &text);
    /// Emitted when the panel's close button hides it, so the View
    /// menu toggle stays in sync.
    void panelClosed();
    /// Match-bank load request (worker thread).
    void requestLoadModel(const QString &path);
    /// Score request (worker thread): strokes by value, echoed
    /// generation id for staleness checks.
    void requestRecognize(QVector<QVector<QPointF>> strokes, quint32 topK, bool includeKana,
                          quint64 generation);

protected:
    void paintEvent(QPaintEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    /// Persist the size (position is re-anchored to the open button
    /// on every show, so only the size is stored).
    void resizeEvent(QResizeEvent *event) override;
    /// Snap once geometry is live (the ctor can't: neither the
    /// parent nor the hidden panel is laid out yet).
    void showEvent(QShowEvent *event) override;
    /// Interface font changed (UI scale): re-derive the candidate
    /// strip to match, like the initial build does.
    void changeEvent(QEvent *event) override;

private slots:
    void onCanvasStarted(const QPointF &at255);
    void onCanvasMoved(const QPointF &at255);
    void onCanvasEnded();
    void onRecognize();
    /// Worker reply: apply unless superseded (id mismatch).
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void onRecognized(const std::vector<Recognizer::Hit> &hits, double matchMs, quint64 generation);
    /// Bank load settled: cache the count (a corrupt file reads
    /// as a failed load, like a missing one).
    void onModelLoaded(const QString &path, bool ok, quint64 templateCount);
    void onUndo();
    void onRedo();
    void onClear();
    void onGridToggled(bool on);
    void onKanaToggled(bool on);

private:
    /// Install event filters, feed the canvas, and connect everything.
    /// Runs once per form: initial build and every dev reload.
    void wireUi();
    void refreshCanvas() const;
    void updateStatus();
    /// Clicked (not dragged) candidate: forward its text.
    void onCandidateClicked(QListWidgetItem *item);
    /// Snap a requested size to the square-canvas line (see
    /// `PanelSize.h`), honoring layout minimum then parent bounds.
    void applyPanelSize(int reqW, int reqH);
    /// Square chrome buttons: the 40px design at 100% scale (the
    /// shared fit grows the side with the interface font, keeping
    /// them square).
    static constexpr int chromeButton = 40;
    /// Ask the worker for the data-dir bank when present (the
    /// Dictionaries dialog fetches it when missing).
    void tryLoadModel();

    Ui::RecognizerWidget *ui = nullptr;
    /// Loader-built form (dev only; null with compiled uic).
    QWidget *devForm = nullptr;
    /// Match thread: the worker owns the template bank there; every
    /// bank access goes through the queued request/result signals
    /// (never touch a `Recognizer` on this thread).
    QThread matchThread;
    RecognizerWorker *recognizerWorker = nullptr;
    /// Monotonic query id; results echoing an older id are stale
    /// (strokes changed mid-flight) and dropped on arrival.
    quint64 matchGeneration = 0;
    /// Last loaded template count (worker-bank mirror for the status
    /// line; the bank itself stays on the worker).
    quint64 templateCount = 0;
    /// Bank settled (loaded or definitively missing): gates the
    /// status line, which otherwise keeps its loading/missing text.
    bool modelReady = false;
    /// Kanji hover info (optional `kanji.bin`; outlives the form).
    KanjiInfo *kanjiInfo = nullptr;
    /// Last match time in ms (-1 = never matched).
    double matchMs = -1.0;

    /// Resize state while dragging the corner grip.
    bool resizing = false;
    QPoint resizePressGlobal;
    QSize resizePressSize;

    QVector<QVector<QPointF>> strokes;
    QVector<QPointF> open;
    QVector<QVector<QPointF>> redoStrokes;
    /// Double-click detection for candidates: same text twice
    /// within the platform interval opens the kanji dialog instead
    /// of inserting twice.
    QElapsedTimer lastAcceptAt;
    QString lastAcceptText;
};
