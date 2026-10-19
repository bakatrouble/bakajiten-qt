#include "TestMainWindow.h"

#include <QtTest>

#include <QLineEdit>

#include "core/settings/Settings.h"
#include "ui/dictionary/DictionaryWidget.h"
#include "ui/mainwindow/MainWindow.h"
#include "util/Furigana.h"
#include "util/Romaji.h"
#include "widgets/input/KanaInput.h"
#include "widgets/table/DictionaryTable.h"

#include "dict.hpp"

#include <memory>

void TestMainWindow::dictionaryRomajiConvertsBothSchools() {
    using Romaji::toRomaji;
    // Hepburn core (macrons, shi/chi/tsu/fu/ji).
    QCOMPARE(toRomaji(QStringLiteral("たべる"), RomajiStyle::Hepburn), QStringLiteral("taberu"));
    QCOMPARE(toRomaji(QStringLiteral("すし"), RomajiStyle::Hepburn), QStringLiteral("sushi"));
    QCOMPARE(toRomaji(QStringLiteral("ちず"), RomajiStyle::Hepburn), QStringLiteral("chizu"));
    QCOMPARE(toRomaji(QStringLiteral("ふじ"), RomajiStyle::Hepburn), QStringLiteral("fuji"));
    QCOMPARE(toRomaji(QStringLiteral("せんせい"), RomajiStyle::Hepburn), QStringLiteral("sensei"));
    QCOMPARE(toRomaji(QStringLiteral("とうきょう"), RomajiStyle::Hepburn),
             QString::fromUtf8("t\xc5\x8dky\xc5\x8d")); // tōkyō
    QCOMPARE(toRomaji(QStringLiteral("おおさか"), RomajiStyle::Hepburn),
             QString::fromUtf8("\xc5\x8dsaka")); // ōsaka
    QCOMPARE(toRomaji(QStringLiteral("がくせい"), RomajiStyle::Hepburn), QStringLiteral("gakusei"));
    // Kunrei core (si/ti/tu/hu/zi, circumflexes, ei merged).
    QCOMPARE(toRomaji(QStringLiteral("すし"), RomajiStyle::Kunrei), QStringLiteral("susi"));
    QCOMPARE(toRomaji(QStringLiteral("ちず"), RomajiStyle::Kunrei), QStringLiteral("tizu"));
    QCOMPARE(toRomaji(QStringLiteral("ふじ"), RomajiStyle::Kunrei), QStringLiteral("huzi"));
    QCOMPARE(toRomaji(QStringLiteral("せんせい"), RomajiStyle::Kunrei),
             QString::fromUtf8("sens\xc3\xaa")); // sensê
    QCOMPARE(toRomaji(QStringLiteral("とうきょう"), RomajiStyle::Kunrei),
             QString::fromUtf8("t\xc3\xb4ky\xc3\xb4")); // tôkyô
    // Geminates, ん rules, digraphs, katakana, long mark.
    QCOMPARE(toRomaji(QStringLiteral("きって"), RomajiStyle::Hepburn), QStringLiteral("kitte"));
    QCOMPARE(toRomaji(QStringLiteral("さっか"), RomajiStyle::Hepburn), QStringLiteral("sakka"));
    QCOMPARE(toRomaji(QStringLiteral("ほん"), RomajiStyle::Hepburn), QStringLiteral("hon"));
    QCOMPARE(toRomaji(QStringLiteral("さんぽ"), RomajiStyle::Hepburn), QStringLiteral("sampo"));
    QCOMPARE(toRomaji(QStringLiteral("ほんやく"), RomajiStyle::Hepburn),
             QStringLiteral("hon'yaku"));
    QCOMPARE(toRomaji(QStringLiteral("きょう"), RomajiStyle::Hepburn),
             QString::fromUtf8("ky\xc5\x8d")); // kyō (digraph merges)
    QCOMPARE(toRomaji(QStringLiteral("ラーメン"), RomajiStyle::Hepburn),
             QString::fromUtf8("r\xc4\x81men")); // rāmen
    QCOMPARE(toRomaji(QStringLiteral("テラスハウス"), RomajiStyle::Hepburn),
             QStringLiteral("terasuhausu"));
    // Verb endings stay split (かう, not kō); noun よう is the known
    // exception (renders "you").
    QCOMPARE(toRomaji(QStringLiteral("よう"), RomajiStyle::Hepburn), QStringLiteral("you"));
    QCOMPARE(toRomaji(QStringLiteral("かう"), RomajiStyle::Hepburn), QStringLiteral("kau"));
    QCOMPARE(toRomaji(QStringLiteral("おもう"), RomajiStyle::Hepburn), QStringLiteral("omou"));
}

void TestMainWindow::dictionaryFuriganaSegments() {
    // Greedy outside-in with backtracking + kana folding. Spans
    // read (text, ruby); kana-only and unalignable pairs are
    // nullopt (both render as today's plain heading).
    const auto spans = [](const char *head, const char *reading) {
        return Furigana::segment(QString::fromUtf8(head), QString::fromUtf8(reading));
    };
    const auto texts = [](const std::optional<QVector<Furigana::Span>> &result) {
        QStringList out;
        if (result.has_value()) {
            for (const Furigana::Span &span : *result) {
                out << span.text + QStringLiteral("/") + span.ruby;
            }
        }
        return out;
    };
    QCOMPARE(texts(spans("食べる", "たべる")),
             (QStringList{QStringLiteral("食/た"), QStringLiteral("べる/")}));
    QCOMPARE(
        texts(spans("お母さん", "おかあさん")),
        (QStringList{QStringLiteral("お/"), QStringLiteral("母/かあ"), QStringLiteral("さん/")}));
    QCOMPARE(texts(spans("今日", "きょう")), QStringList{QStringLiteral("今日/きょう")});
    QCOMPARE(texts(spans("限り限り", "ぎりぎり")),
             (QStringList{QStringLiteral("限/ぎ"), QStringLiteral("り/"), QStringLiteral("限/ぎ"),
                          QStringLiteral("り/")}));
    QCOMPARE(texts(spans("ガリ勉", "がりべん")),
             (QStringList{QStringLiteral("ガリ/"), QStringLiteral("勉/べん")}));
    QCOMPARE(texts(spans("掻っ攫う", "かっさらう")),
             (QStringList{QStringLiteral("掻/か"), QStringLiteral("っ/"), QStringLiteral("攫/さら"),
                          QStringLiteral("う/")}));
    QCOMPARE(texts(spans("窺う", "うかがう")),
             (QStringList{QStringLiteral("窺/うかが"), QStringLiteral("う/")}));
    // Kana-only: nothing to annotate.
    QVERIFY(!spans("たべる", "たべる").has_value());
    QVERIFY(!spans("テーブル", "テーブル").has_value());
    // Mismatched pairs: fallback.
    QVERIFY(!spans("たべる", "のむ").has_value());
    QVERIFY(!spans("食べる", "のむ").has_value());
    QVERIFY(!spans("", "たべる").has_value());
    QVERIFY(!spans("食べる", "").has_value());
}

void TestMainWindow::dictionaryReadingFollowsSetting() {
    AppSettings baseline = AppSettings::fromStorage();
    AppSettings shared;
    shared.setReading(ReadingDisplay::Kana);
    shared.save();
    AppSettings::shared().load();
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    auto *table = window->findChild<DictionaryTable *>("resultsTable");
    QVERIFY2(table != nullptr, "results table missing or not promoted");
    auto *input = window->findChild<QLineEdit *>("searchInput");
    QVERIFY2(input != nullptr, "search input missing");
    auto *model = table->model();
    QVERIFY2(model != nullptr, "results model missing");
    auto *dictionary = window->findChild<DictionaryWidget *>();
    QVERIFY2(dictionary != nullptr, "dictionary widget missing");
    input->setText(QStringLiteral("すし"));
    QTest::keyClick(input, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() > 0, 30000);
    // The word itself matches: find its row (file order varies).
    int sushi = -1;
    for (int r = 0; r < model->rowCount(); ++r) {
        if (model->index(r, 1).data(Qt::DisplayRole).toString() == QStringLiteral("すし")) {
            sushi = r;
            break;
        }
    }
    QVERIFY2(sushi >= 0, "すし row missing");
    const auto readingAt = [&](int row) {
        return model->index(row, 1).data(Qt::DisplayRole).toString();
    };
    QCOMPARE(readingAt(sushi), QStringLiteral("すし"));
    shared.setReading(ReadingDisplay::Hepburn);
    shared.save();
    AppSettings::shared().load();
    dictionary->refreshReadings();
    QCOMPARE(readingAt(sushi), QStringLiteral("sushi"));
    shared.setReading(ReadingDisplay::Kunrei);
    shared.save();
    AppSettings::shared().load();
    dictionary->refreshReadings();
    QCOMPARE(readingAt(sushi), QStringLiteral("susi"));
    shared.setReading(ReadingDisplay::Kana);
    shared.save();
    AppSettings::shared().load();
    dictionary->refreshReadings();
    QCOMPARE(readingAt(sushi), QStringLiteral("すし"));
    baseline.save();
    AppSettings::shared().load();
}

void TestMainWindow::dictionaryRomajiBatchConvertsToKana() {
    // Query-edge batch conversion, row-for-row from
    // docs/romaji-kana.md §5 (through the Qt search-text seam,
    // same function onSearch uses).
    const std::vector<std::pair<const char *, const char *>> rows = {
        {"a", "あ"},
        {"shi", "し"},
        {"chi", "ち"},
        {"tsu", "つ"},
        {"fu", "ふ"},
        {"wo", "を"},
        {"n", "ん"},
        {"si", "し"},
        {"ti", "ち"},
        {"tu", "つ"},
        {"hu", "ふ"},
        {"zi", "じ"},
        {"sya", "しゃ"},
        {"tyu", "ちゅ"},
        {"zyo", "じょ"},
        {"di", "ぢ"},
        {"du", "づ"},
        {"dya", "ぢゃ"},
        {"kya", "きゃ"},
        {"shu", "しゅ"},
        {"cho", "ちょ"},
        {"nyu", "にゅ"},
        {"ja", "じゃ"},
        {"byo", "びょ"},
        {"konnichiwa", "こんにちは"},
        {"konbanwa", "こんばんは"},
        {"arigatou", "ありがとう"},
        {"gakkou", "がっこう"},
        {"shinjuku", "しんじゅく"},
        {"matcha", "まっちゃ"},
        {"kissaten", "きっさてん"},
        {"beddo", "べっど"},
        {"emma", "えんま"},
        {"komatta", "こまった"},
        {"kinen", "きねん"},
        {"ginen", "ぎねん"},
        {"kin'en", "きんえん"},
        {"shin", "しん"},
        {"hon", "ほん"},
        {"shinya", "しにゃ"},
        {"shin'ya", "しんや"},
        {"xtsu", "っ"},
        {"ltu", "っ"},
        {"xa", "ぁ"},
        {"xyo", "ょ"},
        {"xwa", "ゎ"},
        {"fa", "ふぁ"},
        {"vu", "ゔ"},
        {"thi", "てぃ"},
        {"dhu", "でゅ"},
        {"wi", "うぃ"},
        {"ye", "いぇ"},
        {"pa-thi", "ぱーてぃ"},
        {"食べる", "食べる"},
        {"あ", "あ"},
        {"漢字", "漢字"},
        {"taberu食べる", "たべる食べる"},
        {"hello", "へlぉ"},
        {"'", "'"},
    };
    for (const auto &[input, expected] : rows) {
        QCOMPARE(KanaInput::searchText(QString::fromUtf8(input)), QString::fromUtf8(expected));
    }
}

void TestMainWindow::dictionaryRomajiDisplayShowsPendingTail() {
    // Incremental display, row-for-row from docs/ime-conversion.md
    // §4: settled kana plus the pending live tail shown raw.
    const std::vector<std::pair<const char *, const char *>> rows = {
        {"s", "s"},
        {"sh", "sh"},
        {"shi", "し"},
        {"n", "n"},
        {"na", "な"},
        {"nn", "んn"},
        {"nna", "んな"},
        {"t", "t"},
        {"tc", "tc"},
        {"tch", "tch"},
        {"tchi", "っち"},
        {"shin", "しn"},
        {"shina", "しな"},
        {"shinya", "しにゃ"},
        {"shin'a", "しんあ"},
        {"ka", "か"},
        {"kat", "かt"},
        {"katta", "かった"},
        {"katt", "かっt"},
        {"gakk", "がっk"},
        {"ko", "こ"},
        {"kon", "こn"},
        {"konn", "こんn"},
        {"konni", "こんに"},
        {"konnichi", "こんにち"},
        {"konnichiw", "こんにちw"},
        {"konnichiwa", "こんにちは"},
        {"tsu", "つ"},
        {"du", "づ"},
        {"thi", "てぃ"},
        {"wo", "を"},
        {"-", "ー"},
        {"xtsu", "っ"},
        {"n'", "ん"},
        {"q", "q"},
    };
    for (const auto &[raw, expected] : rows) {
        QCOMPARE(QString::fromStdString(dict::romaji_display(std::string(raw))),
                 QString::fromUtf8(expected));
    }
}
