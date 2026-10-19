#include "TestMainWindow.h"

#include <QtTest>

#include <QLineEdit>
#include <QPalette>
#include <QSignalSpy>

#include "core/dictionary/DictionaryModel.h"
#include "core/dictionary/MeaningBuilder.h"
#include "core/palette/MaterialPalette.h"
#include "ui/mainwindow/MainWindow.h"
#include "widgets/table/DictionaryTable.h"

#include <memory>

void TestMainWindow::dictionaryDeinflectedRowsTinted() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    auto *dictModel = qobject_cast<DictionaryModel *>(model);
    QVERIFY2(dictModel != nullptr, "not a DictionaryModel");
    // のんだ: direct hits (飲んだくれ…) plus the deinflected 飲む.
    QSignalSpy finished(dictModel, &DictionaryModel::searchFinished);
    input->setText(QStringLiteral("のんだ"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 30000);
    QVERIFY(model->rowCount() > 1);
    const auto headingAt = [&](int row) {
        return model->index(row, 0).data(Qt::DisplayRole).toString();
    };
    const auto deinflectedAt = [&](int row) {
        return model->index(row, 0).data(DictionaryModel::DeinflectedRole).toBool();
    };
    int deinf = -1;
    int direct = -1;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (headingAt(row) == QStringLiteral("飲む") && deinf < 0) {
            deinf = row;
        }
        if (!deinflectedAt(row) && direct < 0) {
            direct = row;
        }
    }
    QVERIFY2(deinf >= 0, "no deinflected 飲む row");
    QVERIFY(deinflectedAt(deinf));
    QVERIFY2(direct >= 0, "no direct row");
    // Tinted background on the deinflected row only (sampled in the
    // cell margin, left of any text).
    table->scrollTo(model->index(deinf, 0), QAbstractItemView::EnsureVisible);
    table->scrollTo(model->index(direct, 0), QAbstractItemView::EnsureVisible);
    const QImage painted = table->viewport()->grab().toImage();
    const QRect deinfCell = table->visualRect(model->index(deinf, 0));
    const QRect directCell = table->visualRect(model->index(direct, 0));
    QVERIFY(!deinfCell.isNull() && !directCell.isNull());
    const auto bgAt = [&](const QRect &cell) {
        return painted.pixelColor(cell.left() + 1, cell.center().y());
    };
    const QColor base = table->palette().color(QPalette::Base);
    QCOMPARE(bgAt(directCell), base);
    QVERIFY(bgAt(deinfCell) != base);
}

void TestMainWindow::dictionaryDeinflectedShowsFormName() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    auto *dictModel = qobject_cast<DictionaryModel *>(model);
    QVERIFY2(dictModel != nullptr, "not a DictionaryModel");
    QSignalSpy finished(dictModel, &DictionaryModel::searchFinished);
    const auto inflectionAt = [&](int row) {
        return model->index(row, 2).data(DictionaryModel::InflectionRole).toString();
    };
    // Single hop, zkanji name: たべた → 食べる · past.
    input->setText(QStringLiteral("たべた"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 30000);
    int taberu = -1;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (model->index(row, 0).data(Qt::DisplayRole).toString() == QStringLiteral("食べる")) {
            taberu = row;
            break;
        }
    }
    QVERIFY2(taberu >= 0, "no 食べる row for たべた");
    QCOMPARE(inflectionAt(taberu), QStringLiteral("past"));
    // Stacked hops join with ", " (zkanji's wordInflectionText);
    // connector hops keep engine names.
    input->setText(QStringLiteral("きてください"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 2, 30000);
    int kuru = -1;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (model->index(row, 0).data(Qt::DisplayRole).toString() == QStringLiteral("来る")) {
            kuru = row;
            break;
        }
    }
    QVERIFY2(kuru >= 0, "no 来る row for きてください");
    QCOMPARE(inflectionAt(kuru), QStringLiteral("ください strip, conjunctive"));
    // Direct rows carry no inflection name.
    for (int row = 0; row < model->rowCount(); ++row) {
        if (!model->index(row, 0).data(DictionaryModel::DeinflectedRole).toBool()) {
            QVERIFY(inflectionAt(row).isEmpty());
            break;
        }
    }
    // The name leads the meaning runs as its own small run (paint
    // and hit-testing share the runs, so this covers both).
    QVector<SenseDisplay> senses;
    SenseDisplay sense;
    sense.gloss = QStringLiteral("to eat");
    senses << sense;
    const QVector<MeaningBuilder::Run> runs = MeaningBuilder::build(
        senses, table->font(), QStringLiteral("past"), table->palette(), false);
    QVERIFY(!runs.isEmpty());
    QCOMPARE(runs.first().kind, MeaningBuilder::Kind::Infl);
    QCOMPARE(runs.first().text, QStringLiteral("past "));
    const QVector<MeaningBuilder::Run> direct =
        MeaningBuilder::build(senses, table->font(), QString(), table->palette(), false);
    QVERIFY(!direct.isEmpty());
    QVERIFY(direct.first().kind != MeaningBuilder::Kind::Infl);
    // Lone senses number too ("1. "), matching the article.
    QCOMPARE(direct.first().kind, MeaningBuilder::Kind::Number);
    QCOMPARE(direct.first().text, QStringLiteral("1. "));
}

void TestMainWindow::dictionaryTableHighlightsMatch() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    auto *dictModel = qobject_cast<DictionaryModel *>(model);
    QVERIFY2(dictModel != nullptr, "not a DictionaryModel");
    QSignalSpy finished(dictModel, &DictionaryModel::searchFinished);
    const auto matchAt = [&](int row) {
        return model->index(row, 0).data(DictionaryModel::MatchRole).toString();
    };
    // Deinflected row highlights the candidate surface (たべる),
    // which the heading (食べる) does not contain.
    input->setText(QStringLiteral("たべた"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 30000);
    int taberu = -1;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (model->index(row, 0).data(Qt::DisplayRole).toString() == QStringLiteral("食べる")) {
            taberu = row;
            break;
        }
    }
    QVERIFY2(taberu >= 0, "no 食べる row for たべた");
    QCOMPARE(matchAt(taberu), QStringLiteral("たべる"));
    // Direct row highlights the query itself.
    input->setText(QStringLiteral("のんだ"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 2, 30000);
    int nomu = -1;
    int nondakure = -1;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QString heading = model->index(row, 0).data(Qt::DisplayRole).toString();
        if (heading == QStringLiteral("飲む")) {
            nomu = row;
        }
        if (heading == QStringLiteral("飲んだくれ")) {
            nondakure = row;
        }
    }
    QVERIFY2(nomu >= 0, "no 飲む row for のんだ");
    QVERIFY2(nondakure >= 0, "no 飲んだくれ row for のんだ");
    QCOMPARE(matchAt(nomu), QStringLiteral("のむ"));
    QCOMPARE(matchAt(nondakure), QStringLiteral("のんだ"));
    // Painted: green match text inside the matched reading cell,
    // none in the unmatched heading cell — and no Highlight
    // background anywhere (the text color alone marks the match;
    // fresh たべた rows — the のんだ search above replaced the
    // model).
    input->setText(QStringLiteral("たべた"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 3, 30000);
    taberu = -1;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (model->index(row, 0).data(Qt::DisplayRole).toString() == QStringLiteral("食べる")) {
            taberu = row;
            break;
        }
    }
    QVERIFY2(taberu >= 0, "no 食べる row for たべた (researched)");
    table->scrollTo(model->index(taberu, 0), QAbstractItemView::EnsureVisible);
    const QImage painted = table->viewport()->grab().toImage();
    const QColor highlight = table->palette().color(QPalette::Highlight);
    const auto hasHighlight = [&](const QRect &cell) {
        for (int y = cell.top(); y <= cell.bottom(); ++y) {
            for (int x = cell.left(); x <= cell.right(); ++x) {
                if (painted.pixelColor(x, y) == highlight) {
                    return true;
                }
            }
        }
        return false;
    };
    QVERIFY(!hasHighlight(table->visualRect(model->index(taberu, 0))));
    QVERIFY(!hasHighlight(table->visualRect(model->index(taberu, 1))));
    // Match text reads accent green (both variants inside the
    // palette), in the matched cell only.
    const QColor green = MaterialPalette::fromPalette(table->palette()).accentGreen();
    const auto hasGreen = [&](const QRect &cell) {
        for (int y = cell.top(); y <= cell.bottom(); ++y) {
            for (int x = cell.left(); x <= cell.right(); ++x) {
                if (painted.pixelColor(x, y) == green) {
                    return true;
                }
            }
        }
        return false;
    };
    QVERIFY(!hasGreen(table->visualRect(model->index(taberu, 0))));
    QVERIFY(hasGreen(table->visualRect(model->index(taberu, 1))));
}
