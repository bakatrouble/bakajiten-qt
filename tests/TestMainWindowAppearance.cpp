#include "TestMainWindow.h"

#include <QtTest>

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPalette>
#include <QPushButton>

#include "core/palette/MaterialPalette.h"
#include "core/settings/Settings.h"
#include "ui/mainwindow/MainWindow.h"
#include "widgets/table/DictionaryTable.h"
#include "widgets/text/ArticleView.h"
#include "widgets/text/RubyLabel.h"

#include "ui/recognizer/RecognizerWidget.h"
#include <memory>

void TestMainWindow::toolbarFollowsFont() {
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    QTest::mouseClick(openButtonOf(window.get()), Qt::LeftButton);
    auto *kana = window->findChild<QPushButton *>("kanaButton");
    QVERIFY2(kana != nullptr, "kana button missing");
    // Fixed 40px chrome: the buttons never outgrow the row, but
    // their content font still follows the interface scale.
    const double beforePoint = kana->font().pointSizeF();
    AppSettings baseline;
    baseline.setScale(1.0);
    baseline.apply();
    AppSettings up;
    up.setScale(1.5);
    up.apply();
    // Font propagation and relayout are queued: settle before
    // measuring (the app's event loop does this continuously).
    QTest::qWait(100);
    QVERIFY2(kana->height() <= 40, "toolbar chrome must stay capped");
    QVERIFY2(kana->font().pointSizeF() > beforePoint, "toolbar font did not follow the scale");
    // Square chrome: every toolbar button keeps equal sides.
    auto *toolbar = panelOf(window.get())->findChild<QWidget *>("toolbar");
    QVERIFY2(toolbar != nullptr, "toolbar missing");
    const auto buttons = toolbar->findChildren<QPushButton *>();
    QVERIFY2(!buttons.isEmpty(), "toolbar has no buttons");
    for (auto *button : buttons) {
        QCOMPARE(button->width(), button->height());
    }
    baseline.apply();
}

void TestMainWindow::themeRoundTrip() {
    const QString savedStyle = QApplication::style()->objectName();
    // Style applies live (no relaunch); settings untouched.
    AppSettings fusion;
    fusion.setStyle(QStringLiteral("fusion"));
    fusion.applyStyle();
    QCOMPARE(QApplication::style()->objectName(), QStringLiteral("fusion"));
    // Values round-trip through storage.
    fusion.save();
    QCOMPARE(AppSettings::fromStorage().style(), QStringLiteral("fusion"));
    // Restore the entry style.
    AppSettings entry;
    entry.setStyle(savedStyle);
    entry.save();
    entry.applyStyle();
    QCOMPARE(QApplication::style()->objectName(), savedStyle);
    // "Default" (empty) recreates the captured OS style instead of
    // keeping whatever was applied last.
    AppSettings::noteOsStyle();
    fusion.applyStyle();
    QCOMPARE(QApplication::style()->objectName(), QStringLiteral("fusion"));
    AppSettings def;
    def.save();
    def.applyStyle();
    QCOMPARE(QApplication::style()->objectName(), savedStyle);
}

void TestMainWindow::themeLinkStaysBlue() {
    // Breeze-dark QPalette::Link is near-white lavender (#b7c5f9,
    // probe-verified): the role must stay a real blue on both
    // themes even when the style role is hostile.
    QPalette dark;
    dark.setColor(QPalette::Window, QColor(53, 53, 53));
    dark.setColor(QPalette::Link, QColor(QStringLiteral("#b7c5f9")));
    QCOMPARE(MaterialPalette::fromPalette(dark).link().name(), QStringLiteral("#8ab4f8"));
    QPalette light;
    light.setColor(QPalette::Window, Qt::white);
    light.setColor(QPalette::Link, QColor(QStringLiteral("#b7c5f9")));
    QCOMPARE(MaterialPalette::fromPalette(light).link().name(), QStringLiteral("#0000ff"));
}

void TestMainWindow::themeWarningStaysRed() {
    // The `uk` marker reads warning-red on both themes (fixed
    // hexes, never a style role).
    QPalette dark;
    dark.setColor(QPalette::Window, QColor(53, 53, 53));
    QCOMPARE(MaterialPalette::fromPalette(dark).warning().name(), QStringLiteral("#e57373"));
    QPalette light;
    light.setColor(QPalette::Window, Qt::white);
    QCOMPARE(MaterialPalette::fromPalette(light).warning().name(), QStringLiteral("#c62828"));
}

void TestMainWindow::themeStrokeBadgeRampsBlueToGreen() {
    // Badge fills ramp blue (first) → green (last), monotonic in
    // hue on either theme (computed ramp, never a style role).
    const MaterialPalette theme = MaterialPalette::fromPalette(QApplication::palette());
    constexpr int total = 5;
    double lastHue = 1.0;
    for (int i = 0; i < total; ++i) {
        const double hue = theme.strokeBadge(i, total).hslHueF();
        QVERIFY2(hue >= 0.0, "badge hue invalid");
        QVERIFY2(hue < lastHue, "badge ramp not decreasing");
        lastHue = hue;
    }
    const double first = theme.strokeBadge(0, total).hslHueF();
    const double last = theme.strokeBadge(total - 1, total).hslHueF();
    QVERIFY2(first > 0.55 && first < 0.70, "first badge not blue");
    QVERIFY2(last > 0.28 && last < 0.40, "last badge not green");
    // Degenerate totals stay blue, never divide by zero.
    QCOMPARE(theme.strokeBadge(0, 1).hslHueF(), first);
    QCOMPARE(theme.strokeBadge(0, 0).hslHueF(), first);
}

void TestMainWindow::iconsResolveFromTheme() {
    // Explicit choices pass through untouched.
    AppSettings forced;
    forced.setIcons(IconScheme::Light);
    QCOMPARE(forced.resolvedIcons(), IconScheme::Light);
    forced.setIcons(IconScheme::Dark);
    QCOMPARE(forced.resolvedIcons(), IconScheme::Dark);
    // Auto derives from the theme: Fusion's standard palette is
    // light, so icons come out dark.
    AppSettings automatic;
    automatic.setStyle(QStringLiteral("fusion"));
    automatic.setIcons(IconScheme::Auto);
    QCOMPARE(automatic.resolvedIcons(), IconScheme::Dark);
}

void TestMainWindow::dictionaryKanjiCardFollowsTheme() {
    // Card chrome uses stylesheet palette() roles, which bake at
    // polish time: after a theme switch the card must re-resolve
    // instead of keeping the old theme's tooltip-base.
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    auto *tip = window->findChild<QWidget *>("kanjiTip");
    QVERIFY2(tip != nullptr, "kanji hover card missing");
    const QRect cell = table->visualRect(model->index(0, 0));
    QVERIFY(!cell.isNull());
    const QPoint at = cell.topLeft() + QPoint(6, cell.height() / 2);
    QMouseEvent hover(QEvent::MouseMove, QPointF(at), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(table->viewport(), &hover);
    QTest::qWait(450);
    QTRY_VERIFY_WITH_TIMEOUT(tip->isVisible(), 5000);
    auto *card = tip->findChild<QWidget *>("tooltipCard");
    QVERIFY2(card != nullptr, "tooltip card missing");
    // Right-edge padding (labels are centered, so this is always
    // card background — never glyph ink).
    const auto cardTone = [card] {
        const QPoint sample(card->width() - 8, card->height() / 2);
        return card->grab().toImage().pixelColor(sample).lightness();
    };
    // Distinctive themes both ways (assert failures return early,
    // so the guard owns the restore).
    struct PaletteGuard {
        QPalette saved = QApplication::palette();
        ~PaletteGuard() { QApplication::setPalette(saved); }
    } guard;
    QPalette light = guard.saved;
    light.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#FFFFDC")));
    light.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#000000")));
    QApplication::setPalette(light);
    QTRY_VERIFY_WITH_TIMEOUT(cardTone() > 150, 5000);
    QPalette dark = guard.saved;
    dark.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#232323")));
    dark.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#FFFFFF")));
    QApplication::setPalette(dark);
    QTRY_VERIFY_WITH_TIMEOUT(cardTone() < 100, 5000);
}

void TestMainWindow::dictionaryArticleRerendersOnPaletteChange() {
    // Theme switch with an article open: run paints resolve pens
    // from the live palette (nothing baked), and badges re-apply
    // stylesheets through refreshTheme — so switched ink shows up
    // without rebuilding.
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *article = window->findChild<ArticleView *>("articleView");
    QVERIFY2(article != nullptr, "article pane missing");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    input->setText(QStringLiteral("たべる"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(table->model()->rowCount() > 0, 30000);
    table->selectRow(0);
    QTRY_VERIFY_WITH_TIMEOUT(article->isVisible(), 5000);
    auto *heading = article->findChild<RubyLabel *>(QStringLiteral("articleHeading"));
    QVERIFY2(heading != nullptr, "article heading missing");
    auto *jlpt = article->findChild<QLabel *>(QStringLiteral("jlptBadge"));
    QVERIFY2(jlpt != nullptr, "JLPT badge missing");
    const auto hasInk = [&](const QColor &ink) {
        const QImage pixels = heading->grab().toImage();
        for (int y = 0; y < pixels.height(); ++y) {
            for (int x = 0; x < pixels.width(); ++x) {
                if (pixels.pixelColor(x, y) == ink) {
                    return true;
                }
            }
        }
        return false;
    };
    // A distinctive theme, restored on scope exit (assert failures
    // return early).
    struct PaletteGuard {
        QPalette saved = QApplication::palette();
        ~PaletteGuard() { QApplication::setPalette(saved); }
    } guard;
    QPalette switched = guard.saved;
    switched.setColor(QPalette::Window, QColor(53, 53, 53));
    switched.setColor(QPalette::Text, QColor(QStringLiteral("#123456")));
    switched.setColor(QPalette::Link, QColor(QStringLiteral("#ABCDEF")));
    QApplication::setPalette(switched);
    QApplication::processEvents();
    QTRY_VERIFY_WITH_TIMEOUT(hasInk(QColor(QStringLiteral("#123456"))), 5000);
    // Badge fill follows the link role (fixed blue per theme), not
    // the style's Link — re-applied through refreshTheme.
    QTRY_VERIFY_WITH_TIMEOUT(
        jlpt->styleSheet().contains(QStringLiteral("#8ab4f8"), Qt::CaseInsensitive), 5000);
    QApplication::setPalette(guard.saved);
    QApplication::processEvents();
    QTRY_VERIFY_WITH_TIMEOUT(hasInk(guard.saved.color(QPalette::Text)), 5000);
}
