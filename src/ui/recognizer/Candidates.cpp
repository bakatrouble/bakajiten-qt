#include "RecognizerWidget.h"

#include "ui_RecognizerWidget.h"
#include "widgets/list/CandidateList.h"

#include <QApplication>
#include <QListWidget>

/// Candidate strip: matching, click-to-accept, and the status line.
/// Queries run on stroke end, undo/redo, kana toggle, and dev
/// reload; scoring happens on the match thread and each reply
/// rebuilds the cells and refreshes the status (stale replies —
/// strokes changed mid-flight — are dropped on arrival). Scores
/// show as native item tooltips, kanji info as hover cards; both
/// live in CandidateList. List behavior itself (cells, scrolling,
/// fades) lives there too.

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
        return; // loading/missing text already set
    }
    QString text = tr("%1 templates · %2 stroke(s)").arg(templateCount).arg(strokes.size());
    if (matchMs >= 0.0) {
        text += tr(" · %1 ms").arg(matchMs, 0, 'f', 1);
    }
    ui->statusLabel->setText(text);
    ui->undoButton->setEnabled(!strokes.isEmpty());
    ui->redoButton->setEnabled(!redoStrokes.isEmpty());
}

void RecognizerWidget::onRecognize() {
    // Score on the match thread; the id lets the reply prove it is
    // still current (a newer stroke invalidates anything in flight).
    ++matchGeneration;
    emit requestRecognize(strokes, 20, ui->kanaButton->isChecked(), matchGeneration);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void RecognizerWidget::onRecognized(const std::vector<Recognizer::Hit> &hits, double matchMsIn,
                                    quint64 generation) {
    if (generation != matchGeneration) {
        return; // superseded mid-flight: a newer query owns the strip
    }
    matchMs = matchMsIn;
    QVector<CandidateHit> shown;
    shown.reserve(static_cast<int>(hits.size()));
    for (const Recognizer::Hit &hit : hits) {
        shown.append({hit.text, static_cast<double>(hit.score)});
    }
    ui->candidatesList->setHits(shown);
    updateStatus();
}
