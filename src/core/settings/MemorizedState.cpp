#include "core/settings/MemorizedState.h"

#include "core/settings/Settings.h"
#include "core/settings/YamlFormat.h"

#include <QSettings>

namespace {
QSettings stateFile() { return QSettings(AppSettings::memorizedPath(), yamlFormat()); }

QString kanjiKey(MemorizedState::KanjiStrip strip) {
    switch (strip) {
    case MemorizedState::KanjiStrip::Similar:
        return QStringLiteral("kanji/similarOpen");
    case MemorizedState::KanjiStrip::Parts:
        return QStringLiteral("kanji/partsOpen");
    case MemorizedState::KanjiStrip::PartOf:
        return QStringLiteral("kanji/partOfOpen");
    }
    return {};
}
} // namespace

int MemorizedState::resultsHeadingWidth() {
    return stateFile().value(QStringLiteral("results/headingWidth"), 0).toInt();
}

void MemorizedState::setResultsHeadingWidth(int width) {
    stateFile().setValue(QStringLiteral("results/headingWidth"), width);
}

int MemorizedState::resultsReadingWidth() {
    return stateFile().value(QStringLiteral("results/readingWidth"), 0).toInt();
}

void MemorizedState::setResultsReadingWidth(int width) {
    stateFile().setValue(QStringLiteral("results/readingWidth"), width);
}

int MemorizedState::articleWidth() {
    return stateFile().value(QStringLiteral("article/width"), 0).toInt();
}

void MemorizedState::setArticleWidth(int width) {
    stateFile().setValue(QStringLiteral("article/width"), width);
}

void MemorizedState::clearArticleWidth() { stateFile().remove(QStringLiteral("article/width")); }

void MemorizedState::rescaleWidths(double factor) {
    if (!(factor > 0.0) || qFuzzyCompare(factor + 1.0, 2.0)) {
        return;
    }
    QSettings state = stateFile();
    for (const QString &key :
         {QStringLiteral("results/headingWidth"), QStringLiteral("results/readingWidth"),
          QStringLiteral("article/width")}) {
        const int width = state.value(key, 0).toInt();
        if (width > 0) {
            state.setValue(key, qRound(width * factor));
        }
    }
}

QSize MemorizedState::panelSize() {
    return stateFile().value(QStringLiteral("panel/size")).toSize();
}

void MemorizedState::setPanelSize(const QSize &size) {
    stateFile().setValue(QStringLiteral("panel/size"), size);
}

bool MemorizedState::kanjiStripOpen(KanjiStrip strip) {
    return stateFile().value(kanjiKey(strip), false).toBool();
}

void MemorizedState::setKanjiStripOpen(KanjiStrip strip, bool open) {
    stateFile().setValue(kanjiKey(strip), open);
}

QString MemorizedState::skippedUpdateVersion() {
    return stateFile().value(QStringLiteral("update/skipped"), QString()).toString();
}

void MemorizedState::setSkippedUpdateVersion(const QString &version) {
    stateFile().setValue(QStringLiteral("update/skipped"), version);
}
