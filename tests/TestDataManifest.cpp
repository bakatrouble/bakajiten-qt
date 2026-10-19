#include "TestDataManifest.h"

#include <QTemporaryDir>
#include <QtTest>

#include "core/data/DataManifest.h"

void TestDataManifest::parseReadsEntries() {
    const QByteArray bytes = R"({"version": 1, "build_date": "2026-10-17",
        "files": {"jmdict.bin": {"size": 11, "gz": 5, "version": "2026-10-17",
        "sha256": "abc", "sources": ["jmdict"],
        "last_changed_manifest": "http://example.invalid/m.json"}}})";
    const std::optional<DataManifest> manifest = DataManifestReader::parse(bytes);
    QVERIFY(manifest.has_value());
    QCOMPARE(manifest->buildDate, QStringLiteral("2026-10-17"));
    QVERIFY(manifest->files.contains(QStringLiteral("jmdict.bin")));
    const DataFileEntry entry = manifest->files.value(QStringLiteral("jmdict.bin"));
    QCOMPARE(entry.size, 11);
    QCOMPARE(entry.sha256, QStringLiteral("abc"));
    QCOMPARE(entry.sources, QStringList{QStringLiteral("jmdict")});
}

void TestDataManifest::parseRejectsGarbage() {
    QVERIFY(!DataManifestReader::parse(QByteArray()).has_value());
    QVERIFY(!DataManifestReader::parse(QByteArray("not json")).has_value());
    QVERIFY(!DataManifestReader::parse(QByteArray(R"({"version": 1})")).has_value());
    // Entry without a hash is unusable (nothing to verify against).
    QVERIFY(!DataManifestReader::parse(QByteArray(R"({"version": 1, "files":
        {"jmdict.bin": {"size": 11}}})"))
                 .has_value());
}

void TestDataManifest::statusRanksLocalFiles() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString missing = dir.filePath(QStringLiteral("missing.bin"));
    const QString shortFile = dir.filePath(QStringLiteral("short.bin"));
    const QString staleFile = dir.filePath(QStringLiteral("stale.bin"));
    const QString goodFile = dir.filePath(QStringLiteral("good.bin"));
    {
        QFile shortOut(shortFile);
        QVERIFY(shortOut.open(QIODevice::WriteOnly));
        shortOut.write("short");
        QFile staleOut(staleFile);
        QVERIFY(staleOut.open(QIODevice::WriteOnly));
        staleOut.write("stale-bytes!");
        QFile goodOut(goodFile);
        QVERIFY(goodOut.open(QIODevice::WriteOnly));
        goodOut.write("eleven-bytes");
    }
    DataFileEntry entry;
    entry.size = 12;
    entry.sha256 = DataManifestReader::sha256Of(goodFile);
    QVERIFY(!entry.sha256.isEmpty());
    QCOMPARE(DataManifestReader::status(entry, missing), DataFileStatus::Missing);
    QCOMPARE(DataManifestReader::status(entry, shortFile), DataFileStatus::Outdated);
    QCOMPARE(DataManifestReader::status(entry, staleFile), DataFileStatus::Outdated);
    QCOMPARE(DataManifestReader::status(entry, goodFile), DataFileStatus::Current);
}
