#include "TestMainWindow.h"

#include <QtTest>

#include <QApplication>
#include <QCursor>
#include <QFontMetrics>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>

#include "core/palette/MaterialPalette.h"
#include "core/recognizer/ModelPack.h"
#include "core/recognizer/Recognizer.h"
#include "core/settings/Settings.h"
#include "ui/data/DictionaryManagerDialog.h"
#include "ui/data/DictionaryManagerRow.h"
#include "ui/mainwindow/MainWindow.h"
#include "widgets/list/CandidateList.h"

#include "ui/recognizer/RecognizerWidget.h"
#include <memory>

void TestMainWindow::candidateStripShowsHits() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    // The panel never downloads: the Dictionaries dialog is the
    // only surface that fetches the Japanese pack (real
    // dl.google.com download, cached under the data dir
    // afterwards). Drive it like a user would. Heap-allocated:
    // the dialog deletes itself on close.
    auto *modelDialog = new DictionaryManagerDialog(window.get());
    modelDialog->show();
    QVERIFY(QTest::qWaitForWindowExposed(modelDialog));
    DictionaryManagerRow *modelRow = nullptr;
    for (auto *row : modelDialog->findChildren<DictionaryManagerRow *>()) {
        if (row->dataFile() == QStringLiteral("handwriting")) {
            modelRow = row;
        }
    }
    QVERIFY2(modelRow != nullptr, "handwriting row missing");
    const QString modelDir = ModelPacks::rootDir();
    if (!ModelPacks::isCached()) {
        QPushButton *download = nullptr;
        for (auto *button : modelRow->findChildren<QPushButton *>()) {
            if (button->text() != QStringLiteral("Remove")) {
                download = button;
            }
        }
        QVERIFY2(download != nullptr, "handwriting download button missing");
        QVERIFY(download->isEnabled());
        QTest::mouseClick(download, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(ModelPacks::isCached(), 180000);
    }
    modelDialog->close();
    auto *panel = panelOf(window.get());
    panel->reloadModel();
    QTest::mouseClick(openButtonOf(window.get()), Qt::LeftButton);
    auto *canvas = panel->findChild<QWidget *>("canvas");
    QVERIFY2(canvas != nullptr, "canvas missing");
    const QPoint from(40, 40);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, from);
    QTest::mouseMove(canvas, from + QPoint(60, 0));
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, from + QPoint(60, 0));
    auto *list = panel->findChild<QListWidget *>("candidatesList");
    QVERIFY2(list != nullptr, "candidates list missing");
    // Scoring runs on the worker thread against the cached pack
    // (fetched above through the dialog): the strip stays empty
    // until the full beam ranking lands. A horizontal stroke
    // decodes to kanji (一); every cell is plain, hover inverts.
    QTRY_VERIFY_WITH_TIMEOUT(list->count() > 0, 60000);
    // The beam can rank an empty-text hypothesis; cells that show
    // text must be non-empty and plain.
    int shown = -1;
    for (int i = 0; i < list->count(); ++i) {
        if (!list->item(i)->text().isEmpty()) {
            shown = i;
            break;
        }
    }
    QVERIFY2(shown >= 0, "no decoded text in the strip");
    // Cells are plain window colors: the probed corner matches the
    // list background exactly (no tint fill).
    QApplication::processEvents();
    list->scrollTo(list->model()->index(shown, 0));
    QApplication::processEvents();
    const QImage shot = list->viewport()->grab().toImage();
    const QRect cell = list->visualRect(list->model()->index(shown, 0));
    QCOMPARE(shot.pixelColor(cell.topLeft() + QPoint(3, 3)).rgb(),
             list->palette().color(QPalette::Window).rgb());
    // Hover feedback is theme-dependent (sent directly: offscreen
    // cannot raise the popup, so routed mouse moves never reach
    // its viewport — same reason the drag test below synthesizes
    // its own events).
    QMouseEvent hoverMove(QEvent::MouseMove, cell.center(), Qt::NoButton, Qt::NoButton,
                          Qt::NoModifier);
    QApplication::sendEvent(list->viewport(), &hoverMove);
    QTest::qWait(100);
    const QImage hovered = list->viewport()->grab().toImage();
    const QColor corner = hovered.pixelColor(cell.topLeft() + QPoint(3, 3));
    if (MaterialPalette::isDarkPalette(list->palette())) {
        // Dark theme keeps the classic invert.
        QCOMPARE(corner.rgb(), list->palette().color(QPalette::WindowText).rgb());
    } else {
        // Light theme never inverts: a light wash over the window
        // color, glyph stays dark.
        QVERIFY2(corner != list->palette().color(QPalette::Window), qPrintable(corner.name()));
        QVERIFY2(corner.lightnessF() > 0.7, qPrintable(corner.name()));
        int darkest = 255;
        for (int y = cell.top(); y < cell.bottom(); ++y) {
            for (int x = cell.left(); x < cell.right(); ++x) {
                darkest = qMin(darkest, qGray(hovered.pixel(x, y)));
            }
        }
        QVERIFY2(darkest < 128, qPrintable(QString::number(darkest)));
    }
}

void TestMainWindow::candidateClickInsertsQuery() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    QTest::mouseClick(openButtonOf(window.get()), Qt::LeftButton);
    auto *list = panelOf(window.get())->findChild<QListWidget *>("candidatesList");
    QVERIFY2(list != nullptr, "candidates list missing");
    list->addItem(QStringLiteral("あ"));
    QApplication::processEvents();
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    // Plain click (no drag): the text lands in the search field and
    // searches. Starts from an empty field so the append lands
    // exactly (the widget ships a default query).
    input->clear();
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                      list->visualRect(list->model()->index(0, 0)).center());
    QCOMPARE(input->text(), QStringLiteral("あ"));
    auto *status = window->findChild<QLabel *>("statusLabel");
    QVERIFY2(status != nullptr, "status label missing");
    // Real backend on the dict thread: rows (and the hit count)
    // land a queued delivery later. Cold artifact load (hundreds of
    // MB) can take seconds, hence the generous timeout.
    QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("hits")), 30000);
}

void TestMainWindow::candidateInsertsAtCursor() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    QTest::mouseClick(openButtonOf(window.get()), Qt::LeftButton);
    auto *list = panelOf(window.get())->findChild<QListWidget *>("candidatesList");
    QVERIFY2(list != nullptr, "candidates list missing");
    list->addItem(QStringLiteral("あ"));
    QApplication::processEvents();
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    // Mid-text cursor: the pick lands there, not at the end, and
    // focus returns to the field for continued typing.
    input->setText(QStringLiteral("かき"));
    input->setCursorPosition(1);
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                      list->visualRect(list->model()->index(0, 0)).center());
    QCOMPARE(input->text(), QStringLiteral("かあき"));
    QCOMPARE(input->cursorPosition(), 2);
    QVERIFY(input->hasFocus());
}

void TestMainWindow::candidatesFollowScale() {
    // Normalize first (a failed earlier test can leave 1.5 applied).
    AppSettings baseline;
    baseline.setScale(1.0);
    baseline.apply();
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    QTest::mouseClick(openButtonOf(window.get()), Qt::LeftButton);
    auto *list = panelOf(window.get())->findChild<QListWidget *>("candidatesList");
    QVERIFY2(list != nullptr, "candidates list missing");
    // 1.5x the interface font, rows fitted to it — never fixed.
    QVERIFY(qFuzzyCompare(list->font().pointSizeF(), QApplication::font().pointSizeF() * 1.5));
    const int shortRow = list->minimumHeight();
    QVERIFY(shortRow > 0);
    // Live interface-font changes re-derive (same path as builds).
    AppSettings up;
    up.setScale(1.5);
    up.apply();
    QTest::qWait(100);
    QVERIFY(list->font().pointSizeF() > QApplication::font().pointSizeF());
    QVERIFY(list->minimumHeight() > shortRow);
    baseline.apply();
}

void TestMainWindow::candidateScaleStaysIdempotent() {
    // The list font derives from the interface font (never its own
    // explicit one): repeat calls never compound 2x into 4x.
    CandidateList list;
    list.refreshScale();
    const double once = list.font().pointSizeF();
    QVERIFY(once > QApplication::font().pointSizeF());
    list.refreshScale();
    QCOMPARE(list.font().pointSizeF(), once);
}

void TestMainWindow::candidateHoverShowsGlyphUnderCursor() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    // A search first: the shared kanji store (behind every hover
    // card) loads with the dictionaries.
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    input->setText(QStringLiteral("日本"));
    QTest::keyClick(input, Qt::Key_Return);
    QTest::mouseClick(openButtonOf(window.get()), Qt::LeftButton);
    auto *list = panelOf(window.get())->findChild<CandidateList *>("candidatesList");
    QVERIFY2(list != nullptr, "candidates list missing");
    // Multi-character candidate: the card must resolve the glyph
    // under the cursor, not the whole string (which matches no
    // single-kanji card, so nothing showed).
    list->setHits({{QStringLiteral("日本"), 1.0}});
    QApplication::processEvents();
    list->scrollTo(list->model()->index(0, 0));
    QApplication::processEvents();
    const QRect cell = list->visualItemRect(list->item(0));
    QVERIFY(!cell.isNull());
    auto *tip = window->findChild<QWidget *>("kanjiTip");
    QVERIFY2(tip != nullptr, "kanji hover card missing");
    // Cells center their text (candidate delegate paint): measure
    // from the centered left, like the hover path itself.
    const QFontMetrics metrics(list->font());
    const QString text = QStringLiteral("日本");
    const int textLeft = cell.left() + (cell.width() - metrics.horizontalAdvance(text)) / 2;
    const auto hoverGlyph = [&](const QString &want, int x) {
        const QPoint at(x, cell.center().y());
        QCursor::setPos(list->viewport()->mapToGlobal(at));
        QMouseEvent hover(QEvent::MouseMove, QPointF(at), Qt::NoButton, Qt::NoButton,
                          Qt::NoModifier);
        QApplication::sendEvent(list->viewport(), &hover);
        // The card shows after the shared tooltip delay: wait for
        // visibility (exposed means nothing on offscreen), then
        // read the resolved glyph.
        if (!QTest::qWaitFor([&] { return tip->isVisible(); }, 5000)) {
            return false;
        }
        auto *glyph = tip->findChild<QLabel *>("kanjiGlyph");
        return glyph != nullptr && glyph->text() == want;
    };
    // Second glyph first (the case that showed nothing at all).
    QVERIFY2(hoverGlyph(QStringLiteral("本"),
                        textLeft + metrics.horizontalAdvance(QStringLiteral("日")) + 2),
             "no card for the second glyph");
    QVERIFY2(hoverGlyph(QStringLiteral("日"), textLeft + 2), "no card for the first glyph");
}
