#include "RecognizerWidget.h"

#include "ui_RecognizerWidget.h"
#include "widgets/canvas/CanvasWidget.h"
#include "widgets/list/CandidateList.h"

#include <QApplication>
#include <QListWidget>

/// Candidate strip: matching, click-to-accept, and the status line.
/// Queries run on stroke end, undo/redo, and dev reload; scoring
/// happens on the match thread and each reply rebuilds the cells
/// and refreshes the status (stale replies — strokes changed
/// mid-flight — are dropped on arrival). Strokes live in canvas
/// pixels (resize never rescales them) and convert to pen space
/// here, against the live canvas, on every query. Scores
/// show as native item tooltips, kanji info as hover cards; both
/// live in CandidateList. List behavior itself (cells, scrolling,
/// fades) lives there too.

namespace {
/// Canvas pixels → recognizer pen space, mapped in the live
/// canvas geometry.
QVector<QVector<QPointF>> toPenStrokes(CanvasWidget *canvas,
                                       const QVector<QVector<QPointF>> &strokes) {
    QVector<QVector<QPointF>> pen;
    pen.reserve(strokes.size());
    for (const QVector<QPointF> &s : strokes) {
        QVector<QPointF> ps;
        ps.reserve(s.size());
        for (const QPointF &p : s) {
            ps.push_back(canvas->toPen(p));
        }
        pen.push_back(ps);
    }
    return pen;
}
} // namespace

void RecognizerWidget::onCandidateClicked(QListWidgetItem *item) {
    const QString text = item->text();
    if (!lastAcceptText.isEmpty() && text == lastAcceptText && lastAcceptAt.isValid() &&
        !lastAcceptAt.hasExpired(QApplication::doubleClickInterval())) {
        lastAcceptText.clear();
        emit candidateDoubleClicked(text);
        return;
    }
    lastAcceptText = text;
    lastAcceptAt.start();
    emit candidateAccepted(text);
}

void RecognizerWidget::updateStatus() {
    if (!modelReady) {
        return; // loading/downloading text already set
    }
    QString text = tr("%1 stroke(s)").arg(strokes.size());
    if (matchMs >= 0.0) {
        text += tr(" · %1 ms").arg(matchMs, 0, 'f', 1);
    }
    ui->statusLabel->setText(text);
    ui->undoButton->setEnabled(!strokes.isEmpty());
    ui->redoButton->setEnabled(!redoStrokes.isEmpty());
}

void RecognizerWidget::onRecognize() {
    if (strokes.isEmpty()) {
        // Nothing to score: keep the strip empty (undo-to-empty
        // with a bare canvas leaves nothing to decode). Bumping
        // the generation drops in-flight replies that would
        // repopulate it.
        ++matchGeneration;
        ui->candidatesList->clear();
        matchMs = -1.0;
        updateStatus();
        return;
    }
    // The full beam ranking runs on every stroke; the
    // generation lets a newer stroke drop stale replies on
    // arrival.
    ++matchGeneration;
    queryClock.start();
    emit requestRecognize(toPenStrokes(ui->canvas, strokes), 20, matchGeneration);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void RecognizerWidget::onRecognized(const std::vector<Recognizer::Hit> &hits, double /*matchMsIn*/,
                                    quint64 generation) {
    if (generation != matchGeneration) {
        return; // superseded mid-flight: a newer query owns the strip
    }
    // End-to-end: stroke end to cells on screen (queue +
    // decode), so the number matches the felt delay.
    matchMs = static_cast<double>(queryClock.elapsed());
    QVector<CandidateHit> shown;
    shown.reserve(static_cast<int>(hits.size()));
    for (const Recognizer::Hit &hit : hits) {
        shown.append({hit.text, static_cast<double>(hit.score)});
    }
    ui->candidatesList->setHits(shown);
    updateStatus();
}
