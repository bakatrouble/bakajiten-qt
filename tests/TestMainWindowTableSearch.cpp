#include "TestMainWindow.h"

#include <QtTest>

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTableView>

#include "core/dictionary/DictionaryModel.h"
#include "core/dictionary/MeaningBuilder.h"
#include "ui/mainwindow/MainWindow.h"
#include "widgets/table/DictionaryTable.h"

#include <memory>

void TestMainWindow::dictionaryShowsAllSenseTagsAndSkipsEmptyMeanings() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<QTableView *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    // 食べる carries v1 + vt across its senses: structured labels,
    // abbreviated, every sense present (not just the first).
    QCOMPARE(model->index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("食べる"));
    const QVector<SenseDisplay> senses =
        model->index(0, 2).data(DictionaryModel::SensesRole).value<QVector<SenseDisplay>>();
    QVERIFY(!senses.isEmpty());
    QVERIFY(!senses[0].gloss.isEmpty());
    QStringList allPos;
    for (const SenseDisplay &sense : senses) {
        QVERIFY(!sense.gloss.trimmed().isEmpty());
        for (const QString &label : sense.pos) {
            if (!allPos.contains(label)) {
                allPos << label;
            }
        }
    }
    QVERIFY(allPos.contains(QStringLiteral("v1")));
    QVERIFY(allPos.contains(QStringLiteral("vt")));
    // Senses without English glosses leave no empty `|` segments.
    const QString meaning = model->index(0, 2).data(Qt::DisplayRole).toString();
    QVERIFY2(!meaning.isEmpty(), "first row never materialized");
    for (const QString &segment : meaning.split(QStringLiteral(" | "))) {
        QVERIFY2(!segment.trimmed().isEmpty(), qPrintable("empty segment in: " + meaning));
    }
}

void TestMainWindow::dictionarySearchModeTogglesSuffix() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *prefix = window->findChild<QPushButton *>("prefixButton");
    auto *suffix = window->findChild<QPushButton *>("suffixButton");
    QVERIFY2(prefix != nullptr, "prefix button missing");
    QVERIFY2(suffix != nullptr, "suffix button missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    // Prefix is the default mode.
    QVERIFY(prefix->isChecked());
    QVERIFY(!suffix->isChecked());
    input->setText(QStringLiteral("語"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    // Prefix 語: starts-with matches — 日本語 (starts with 日)
    // is nowhere in the results.
    const auto hasJapanese = [&]() -> bool {
        for (int r = 0; r < model->rowCount(); ++r) {
            if (model->index(r, 0).data(Qt::DisplayRole).toString() == QStringLiteral("日本語")) {
                return true;
            }
        }
        return false;
    };
    QVERIFY(!hasJapanese());
    // Suffix on (both toggled): substring mode — 日本語 holds 語
    // mid-key, so it shows up with both buttons checked.
    QTest::mouseClick(suffix, Qt::LeftButton);
    QVERIFY(suffix->isChecked());
    QVERIFY(prefix->isChecked());
    QTRY_VERIFY_WITH_TIMEOUT(hasJapanese(), 30000);
    QVERIFY(hasJapanese());
    // Prefix off (suffix only): still there (suffix edge).
    QTest::mouseClick(prefix, Qt::LeftButton);
    QVERIFY(!prefix->isChecked());
    QVERIFY(suffix->isChecked());
    QTRY_VERIFY_WITH_TIMEOUT(hasJapanese(), 30000);
    QVERIFY(hasJapanese());
    // Suffix off too (neither): exact forms only — 日本語 (partial)
    // leaves, the exact 語 row stays.
    QTest::mouseClick(suffix, Qt::LeftButton);
    QVERIFY(!prefix->isChecked());
    QVERIFY(!suffix->isChecked());
    QTRY_VERIFY_WITH_TIMEOUT(!hasJapanese() && model->rowCount() > 0, 30000);
    QVERIFY(!hasJapanese());
    QVERIFY(model->rowCount() > 0);
}

void TestMainWindow::dictionarySearchModeBothTogglesSubstring() {
    // ほんご is mid-key only (no key starts with it): prefix-only
    // misses 日本語, both toggled finds it through the trigram
    // index.
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *prefix = window->findChild<QPushButton *>("prefixButton");
    auto *suffix = window->findChild<QPushButton *>("suffixButton");
    QVERIFY2(prefix != nullptr, "prefix button missing");
    QVERIFY2(suffix != nullptr, "suffix button missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    input->setText(QStringLiteral("ほんご"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    const auto hasJapanese = [&]() -> bool {
        for (int r = 0; r < model->rowCount(); ++r) {
            if (model->index(r, 0).data(Qt::DisplayRole).toString() == QStringLiteral("日本語")) {
                return true;
            }
        }
        return false;
    };
    QVERIFY(!hasJapanese());
    QTest::mouseClick(suffix, Qt::LeftButton);
    QVERIFY(prefix->isChecked() && suffix->isChecked());
    QTRY_VERIFY_WITH_TIMEOUT(hasJapanese(), 30000);
    QVERIFY(hasJapanese());
}

void TestMainWindow::dictionarySearchModeNeitherIsExact() {
    // たべた deinflects to たべる in every other mode: with neither
    // button on, exact forms only — no deinflection bucket, no
    // partials — and both buttons stay unchecked (no snap-back).
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *prefix = window->findChild<QPushButton *>("prefixButton");
    auto *suffix = window->findChild<QPushButton *>("suffixButton");
    QVERIFY2(prefix != nullptr, "prefix button missing");
    QVERIFY2(suffix != nullptr, "suffix button missing");
    auto *scope = window->findChild<QComboBox *>("scopeCombo");
    QVERIFY2(scope != nullptr, "scope combo missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    input->setText(QStringLiteral("たべた"));
    QTest::keyClick(input, Qt::Key_Return);
    const auto hasTaberu = [&]() -> bool {
        for (int r = 0; r < model->rowCount(); ++r) {
            if (model->index(r, 0).data(Qt::DisplayRole).toString() == QStringLiteral("食べる")) {
                return true;
            }
        }
        return false;
    };
    QTRY_VERIFY_WITH_TIMEOUT(hasTaberu(), 30000);
    // Premise: the prefix hit is deinflected (past), not exact.
    bool sawInflected = false;
    for (int r = 0; r < model->rowCount(); ++r) {
        if (model->index(r, 0).data(Qt::DisplayRole).toString() == QStringLiteral("食べる") &&
            !model->index(r, 2).data(DictionaryModel::InflectionRole).toString().isEmpty()) {
            sawInflected = true;
        }
    }
    QVERIFY(sawInflected);
    // Surnames live in JMnedict: switch scope (the English default
    // holds no exact たべた), then drop to exact.
    scope->setCurrentIndex(1);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    QTest::mouseClick(prefix, Qt::LeftButton);
    QVERIFY(!prefix->isChecked());
    QVERIFY(!suffix->isChecked());
    // Reset lands first (stale rows linger until the worker
    // answers), then the exact tier settles: 食べる gone (no
    // deinflection bucket), surnames named たべた remain.
    QTRY_VERIFY_WITH_TIMEOUT(!hasTaberu() && model->rowCount() > 0, 30000);
    QVERIFY(!hasTaberu());
    // Structural exactness: no row carries deinflection info.
    for (int r = 0; r < model->rowCount(); ++r) {
        QVERIFY2(model->index(r, 2).data(DictionaryModel::InflectionRole).toString().isEmpty(),
                 "deinflected row in exact mode");
    }
    // The dictionary form itself still hits exactly (back in
    // the English scope).
    scope->setCurrentIndex(0);
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(hasTaberu(), 30000);
}
void TestMainWindow::dictionarySearchMeaningMode() {
    // Target toggles: Japanese by default; meaning mode clears the
    // field, darkens the edge buttons, and takes literal English
    // (kana conversion off) to gloss hits. Back to Japanese
    // restores everything.
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *japanese = window->findChild<QPushButton *>("japaneseButton");
    auto *meaning = window->findChild<QPushButton *>("meaningButton");
    QVERIFY2(japanese != nullptr, "japanese button missing");
    QVERIFY2(meaning != nullptr, "meaning button missing");
    auto *prefix = window->findChild<QPushButton *>("prefixButton");
    auto *suffix = window->findChild<QPushButton *>("suffixButton");
    QVERIFY2(prefix != nullptr, "prefix button missing");
    QVERIFY2(suffix != nullptr, "suffix button missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    QVERIFY(japanese->isChecked());
    QVERIFY(!meaning->isChecked());
    QVERIFY(prefix->isEnabled());
    // Toggle boxes: text width fits, height matches the square
    // siblings (the row never grows past 40).
    QCOMPARE(japanese->height(), prefix->height());
    QCOMPARE(meaning->height(), prefix->height());
    QVERIFY(japanese->width() >= japanese->height());
    QVERIFY(meaning->width() >= meaning->height());
    // Japanese baseline first (results to be cleared).
    // Japanese baseline first (results to be cleared).
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    // Switching targets clears the field and the rows, and darkens
    // the edge buttons (meaning search is substring-only).
    QTest::mouseClick(meaning, Qt::LeftButton);
    QVERIFY(!japanese->isChecked());
    QVERIFY(meaning->isChecked());
    QVERIFY(input->text().isEmpty());
    QVERIFY(!prefix->isEnabled());
    QVERIFY(!suffix->isEnabled());
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() == 0, 5000);
    // Real keystrokes (not setText): "eat" must survive literally —
    // with conversion on it would render えあt and never search.
    QTest::keyClicks(input, "eat");
    QCOMPARE(input->text(), QStringLiteral("eat"));
    QTest::keyClick(input, Qt::Key_Return);
    const auto hasTaberu = [&]() -> bool {
        for (int r = 0; r < model->rowCount(); ++r) {
            if (model->index(r, 0).data(Qt::DisplayRole).toString() == QStringLiteral("食べる")) {
                return true;
            }
        }
        return false;
    };
    QTRY_VERIFY_WITH_TIMEOUT(hasTaberu(), 30000);
    // Meaning rows tint nothing (no deinflection bucket here).
    for (int r = 0; r < model->rowCount(); ++r) {
        QVERIFY2(model->index(r, 2).data(DictionaryModel::InflectionRole).toString().isEmpty(),
                 "deinflected row in meaning mode");
    }
    // Clicking the active mode keeps the query (only a switch
    // clears).
    QTest::mouseClick(meaning, Qt::LeftButton);
    QVERIFY(meaning->isChecked());
    QCOMPARE(input->text(), QStringLiteral("eat"));
    // Back to Japanese: edge buttons live again, field cleared,
    // kana conversion back (romaji types to kana).
    QTest::mouseClick(japanese, Qt::LeftButton);
    QVERIFY(japanese->isChecked());
    QVERIFY(!meaning->isChecked());
    QVERIFY(prefix->isEnabled());
    QVERIFY(input->text().isEmpty());
    QTest::keyClicks(input, "taberu");
    QCOMPARE(input->text(), QStringLiteral("たべる"));
}

void TestMainWindow::dictionaryTableShowsMatchedForm() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    const auto headingAt = [&](int row) {
        return model->index(row, 0).data(Qt::DisplayRole).toString();
    };
    // Worker searches land async: one searchFinished per query
    // (row counts alone can't tell stale rows from fresh ones).
    auto *dictModel = qobject_cast<DictionaryModel *>(model);
    QVERIFY2(dictModel != nullptr, "results model not a DictionaryModel");
    QSignalSpy finished(dictModel, &DictionaryModel::searchFinished);
    // Secondary kanji form: the row shows the match (在る), not the
    // entry's first form (有る).
    input->setText(QStringLiteral("在る"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 30000);
    QVERIFY(model->rowCount() > 0);
    bool matched = false;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (headingAt(row) == QStringLiteral("在る")) {
            matched = true;
            break;
        }
    }
    QVERIFY2(matched, "no row shows the matched form 在る");
    // First-form queries are unaffected (same entry, 有る heading).
    input->setText(QStringLiteral("有る"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 2, 30000);
    QVERIFY(model->rowCount() > 0);
    matched = false;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (headingAt(row) == QStringLiteral("有る")) {
            matched = true;
            break;
        }
    }
    QVERIFY2(matched, "no row shows the matched form 有る");
}

void TestMainWindow::dictionarySearchLiveAsYouType() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *status = window->findChild<QLabel *>("statusLabel");
    QVERIFY2(status != nullptr, "status label missing");
    // No Enter: typing alone searches once the debounce fires.
    input->setText(QStringLiteral("日本語能力試験"));
    QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("hits")), 30000);
    // Erasing clears without Enter too.
    input->clear();
    QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("0 hits")), 5000);
}
void TestMainWindow::dictionarySearchScopeCombo() {
    // Scope combo (English/Names/Kokugo, English by default):
    // exactly one dictionary is loaded at a time; switching swaps
    // the file and re-searches without clearing the field. たなか
    // hits words (田中 rice field) and names (Tanaka surname); the
    // surname tag tells them apart.
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *scope = window->findChild<QComboBox *>("scopeCombo");
    QVERIFY2(scope != nullptr, "scope combo missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    QCOMPARE(scope->count(), 3);
    QCOMPARE(scope->currentIndex(), 0);
    input->setText(QStringLiteral("たなか"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    const auto tagsOf = [&](int row) -> QStringList {
        const QVector<SenseDisplay> senses =
            model->index(row, 2).data(DictionaryModel::SensesRole).value<QVector<SenseDisplay>>();
        QStringList tags;
        for (const SenseDisplay &sense : senses) {
            tags << sense.pos;
        }
        return tags;
    };
    const auto hasSurname = [&]() -> bool {
        for (int r = 0; r < model->rowCount(); ++r) {
            if (tagsOf(r).contains(QStringLiteral("surname"))) {
                return true;
            }
        }
        return false;
    };
    const auto hasWordNoun = [&]() -> bool {
        for (int r = 0; r < model->rowCount(); ++r) {
            if (tagsOf(r).contains(QStringLiteral("n"))) {
                return true;
            }
        }
        return false;
    };
    // English default: words only, no surnames.
    QVERIFY2(hasWordNoun(), "no word row in English scope");
    QVERIFY2(!hasSurname(), "surname row in English scope");
    // Names: surnames only, field kept.
    scope->setCurrentIndex(1);
    QTRY_VERIFY_WITH_TIMEOUT(hasSurname() && !hasWordNoun(), 30000);
    QCOMPARE(input->text(), QStringLiteral("たなか"));
    QVERIFY(model->rowCount() > 0);
    // Kokugo: present here (packed alongside), so it loads and
    // searches Japanese definitions; absent files darken instead.
    {
        auto *itemModel = qobject_cast<QStandardItemModel *>(scope->model());
        QVERIFY2(itemModel != nullptr, "combo model missing");
        auto *kokugo = itemModel->item(2);
        QVERIFY2(kokugo != nullptr, "kokugo option missing");
        if (kokugo->isEnabled()) {
            scope->setCurrentIndex(2);
            QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
        }
    }
    // Back to English restores words without surnames.
    scope->setCurrentIndex(0);
    QTRY_VERIFY_WITH_TIMEOUT(hasWordNoun() && !hasSurname(), 30000);
}
