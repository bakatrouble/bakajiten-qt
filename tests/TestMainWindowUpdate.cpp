#include "TestMainWindow.h"

#include <QtTest>

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTemporaryDir>

#include "core/settings/MemorizedState.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/update/UpdateChecker.h"
#include "ui/update/UpdateInstaller.h"
#include "util/Archive.h"

#include "ui/update/UpdateProgress.h"
#include "ui/update/UpdatePrompt.h"
#include <zlib.h>

#include <memory>

void TestMainWindow::updaterVersionCompare() {
    // Tag/current comparison (`v` tolerated, garbage never newer).
    QVERIFY(UpdateChecker::isNewer(QStringLiteral("0.1.0"), QStringLiteral("v0.2.0")));
    QVERIFY(UpdateChecker::isNewer(QStringLiteral("0.1.0"), QStringLiteral("0.2.0")));
    QVERIFY(!UpdateChecker::isNewer(QStringLiteral("0.2.0"), QStringLiteral("v0.2.0")));
    QVERIFY(!UpdateChecker::isNewer(QStringLiteral("0.2.0"), QStringLiteral("v0.1.0")));
    QVERIFY(!UpdateChecker::isNewer(QStringLiteral(""), QStringLiteral("v0.2.0")));
    QVERIFY(!UpdateChecker::isNewer(QStringLiteral("0.1.0"), QStringLiteral("nightly")));
    QVERIFY(UpdateChecker::isNewer(QStringLiteral("0.1.0"), QStringLiteral("0.10.0")));
}

void TestMainWindow::updaterAssetMatch() {
    // Artifact kind picks the release asset by suffix.
    const QStringList names = {
        QStringLiteral("Bakajiten-x86_64.AppImage"), QStringLiteral("bakajiten-win64-v0.1.0.zip"),
        QStringLiteral("bakajiten-linux64-static.tar.gz"), QStringLiteral("SHASUMS.txt")};
    QCOMPARE(UpdateChecker::matchedAsset(names, QStringLiteral("appimage")),
             QStringLiteral("Bakajiten-x86_64.AppImage"));
    QCOMPARE(
        UpdateChecker::matchedAsset({QStringLiteral("bakajiten.exe")}, QStringLiteral("win64")),
        QStringLiteral("bakajiten.exe"));
    QCOMPARE(UpdateChecker::matchedAsset(names, QStringLiteral("linux-static")),
             QStringLiteral("bakajiten-linux64-static.tar.gz"));
    const QStringList macNames = {QStringLiteral("bakajiten-macos-arm64.zip"),
                                  QStringLiteral("bakajiten-macos-x64.zip")};
    QCOMPARE(UpdateChecker::matchedAsset(macNames, QStringLiteral("macos-arm64")),
             QStringLiteral("bakajiten-macos-arm64.zip"));
    QCOMPARE(UpdateChecker::matchedAsset(macNames, QStringLiteral("macos-x64")),
             QStringLiteral("bakajiten-macos-x64.zip"));
    QVERIFY(UpdateChecker::matchedAsset({QStringLiteral("bakajiten-win64-v0.1.0.zip")},
                                        QStringLiteral("appimage"))
                .isEmpty());
    QVERIFY(UpdateChecker::matchedAsset(names, QStringLiteral("")).isEmpty());
    QVERIFY(UpdateChecker::matchedAsset({}, QStringLiteral("appimage")).isEmpty());
}

void TestMainWindow::updaterInstallerKind() {
    // Suffix decides the install path (zip/tarball/bare binary).
    using Payload = UpdateInstaller::Payload;
    QVERIFY(UpdateInstaller::payloadKind(QStringLiteral("bakajiten-win64-v0.1.0.zip")) ==
            Payload::Zip);
    QVERIFY(UpdateInstaller::payloadKind(QStringLiteral("bakajiten-linux64-static.tar.gz")) ==
            Payload::Tarball);
    QVERIFY(UpdateInstaller::payloadKind(QStringLiteral("Bakajiten-x86_64.AppImage")) ==
            Payload::Executable);
    QVERIFY(UpdateInstaller::payloadKind(QStringLiteral("bakajiten-macos-arm64.zip")) ==
            Payload::Zip);
}

void TestMainWindow::updaterSwapScript() {
    // Windows helper waits on our pid, moves every staged file,
    // relaunches, and deletes itself.
    const QString script =
        UpdateInstaller::swapScript(QStringLiteral("C:\\stage"), QStringLiteral("C:\\app"), 1234);
    QVERIFY(script.contains(QStringLiteral("1234")));
    QVERIFY(script.contains(QStringLiteral("C:\\stage")));
    QVERIFY(script.contains(QStringLiteral("C:\\app")));
    QVERIFY(script.contains(QStringLiteral("bakajiten.exe")));
}

namespace {
// Little-endian writers for the zip fixture builder.
void putU16(QByteArray &b, quint16 v) {
    b.append(char(v & 0xFF));
    b.append(char((v >> 8) & 0xFF));
}
void putU32(QByteArray &b, quint32 v) {
    putU16(b, quint16(v & 0xFFFF));
    putU16(b, quint16((v >> 16) & 0xFFFF));
}
// Raw deflate via zlib (strip the zlib header/trailer compress adds).
QByteArray rawDeflate(const QByteArray &in) {
    QByteArray zipped;
    zipped.resize(int(in.size() * 1.1) + 16);
    uLongf len = uLongf(zipped.size());
    if (compress(reinterpret_cast<Bytef *>(zipped.data()), &len,
                 reinterpret_cast<const Bytef *>(in.constData()), uLong(in.size())) != Z_OK) {
        qFatal("cannot deflate test fixture");
    }
    zipped.resize(int(len));
    return zipped.mid(2, int(len) - 6);
}
struct ZipPiece {
    QString name;
    QByteArray data;
    bool deflated = false;
    bool symlink = false;
    bool executable = false;
};
QByteArray buildTestZip(const QVector<ZipPiece> &pieces) {
    QByteArray local;
    QByteArray central;
    quint32 offset = 0;
    for (const ZipPiece &p : pieces) {
        const QByteArray name = p.name.toUtf8();
        const QByteArray payload = p.deflated ? rawDeflate(p.data) : p.data;
        const quint32 crc =
            crc32(0L, reinterpret_cast<const Bytef *>(p.data.constData()), uInt(p.data.size()));
        const quint16 method = p.deflated ? 8 : 0;
        const quint32 mode = (p.symlink ? 0120000u : 0100000u) |
                             (p.executable ? 0755u : (p.symlink ? 0777u : 0644u));
        // Local header.
        local.append("PK\3\4", 4);
        putU16(local, 20);
        putU16(local, 0x0800); // UTF-8 names
        putU16(local, method);
        putU16(local, 0); // time
        putU16(local, 0); // date
        putU32(local, crc);
        putU32(local, quint32(payload.size()));
        putU32(local, quint32(p.data.size()));
        putU16(local, quint16(name.size()));
        putU16(local, 0); // extra
        local.append(name);
        local.append(payload);
        // Central entry.
        central.append("PK\1\2", 4);
        putU16(central, (3 << 8) | 63); // unix made-by
        putU16(central, 20);
        putU16(central, 0x0800);
        putU16(central, method);
        putU16(central, 0);
        putU16(central, 0);
        putU32(central, crc);
        putU32(central, quint32(payload.size()));
        putU32(central, quint32(p.data.size()));
        putU16(central, quint16(name.size()));
        putU16(central, 0);
        putU16(central, 0);
        putU16(central, 0); // disk
        putU16(central, 0); // internal attrs
        putU32(central, mode << 16);
        putU32(central, offset);
        central.append(name);
        offset += quint32(30 + name.size() + payload.size());
    }
    QByteArray zip = local;
    const quint32 cdOffset = quint32(zip.size());
    zip.append(central);
    zip.append("PK\5\6", 4);
    putU16(zip, 0);
    putU16(zip, 0);
    putU16(zip, quint16(pieces.size()));
    putU16(zip, quint16(pieces.size()));
    putU32(zip, quint32(central.size()));
    putU32(zip, cdOffset);
    putU16(zip, 0);
    return zip;
}
QByteArray buildTestTar(const QVector<Archive::Entry> &entries) {
    QByteArray tar;
    for (const Archive::Entry &e : entries) {
        const bool dir = e.path.endsWith(QLatin1Char('/'));
        QByteArray head(512, '\0');
        const QByteArray name = (dir ? e.path.chopped(1) : e.path).toLatin1();
        memcpy(head.data(), name.constData(), qMin(name.size(), 100));
        const QByteArray mode = QByteArray::number(e.executable ? 0755 : 0644, 8);
        memcpy(head.data() + 100, mode.constData(), qMin(mode.size(), 7));
        memcpy(head.data() + 108, "0000000", 7);
        memcpy(head.data() + 116, "0000000", 7);
        const QByteArray size = QByteArray::number(e.data.size(), 8);
        memcpy(head.data() + 124, size.constData(), qMin(size.size(), 11));
        head[156] = dir ? '5' : (e.symlink ? '2' : '0');
        if (e.symlink) {
            memcpy(head.data() + 157, e.data.constData(), qMin(e.data.size(), 100));
        }
        memcpy(head.data() + 257, "ustar", 5);
        quint32 sum = 0;
        memset(head.data() + 148, ' ', 8);
        for (char c : head) {
            sum += static_cast<quint8>(c);
        }
        const QByteArray check = QByteArray::number(sum, 8);
        memcpy(head.data() + 148, check.constData(), qMin(check.size(), 6));
        tar.append(head);
        if (!dir && !e.symlink) {
            tar.append(e.data);
            tar.append(QByteArray((512 - e.data.size() % 512) % 512, '\0'));
        }
    }
    tar.append(QByteArray(1024, '\0'));
    return tar;
}
QByteArray gzipMember(const QByteArray &raw) {
    const QByteArray deflated = rawDeflate(raw);
    const quint32 crc =
        crc32(0L, reinterpret_cast<const Bytef *>(raw.constData()), uInt(raw.size()));
    QByteArray gz("\x1F\x8B\x08\x00\x00\x00\x00\x00\x00\x03", 10);
    gz.append(deflated);
    gz.append(char(crc & 0xFF));
    gz.append(char((crc >> 8) & 0xFF));
    gz.append(char((crc >> 16) & 0xFF));
    gz.append(char((crc >> 24) & 0xFF));
    const quint32 size = quint32(raw.size());
    gz.append(char(size & 0xFF));
    gz.append(char((size >> 8) & 0xFF));
    gz.append(char((size >> 16) & 0xFF));
    gz.append(char((size >> 24) & 0xFF));
    return gz;
}
const Archive::Entry *findEntry(const QVector<Archive::Entry> &entries, const QString &path) {
    for (const Archive::Entry &e : entries) {
        if (e.path == path) {
            return &e;
        }
    }
    return nullptr;
}
} // namespace

void TestMainWindow::updaterArchiveZip() {
    // Fixture mirrors our real payloads: stored + deflated files,
    // an exec bit, a framework-style symlink, a skipped dir and an
    // AppleDouble file.
    const QByteArray body = QByteArray(300, 'x');
    const QByteArray zip = buildTestZip({{"hello.txt", QByteArray("hello")},
                                         {"data.bin", body, true},
                                         {"run.sh", QByteArray("#!/bin/sh\n"), false, false, true},
                                         {"sub/", {}},
                                         {"lib/libfoo.so", QByteArray("elf")},
                                         {"lib/link.so", QByteArray("libfoo.so"), false, true},
                                         {"__MACOSX/._hello", QByteArray("junk")},
                                         {"sub/._ds", QByteArray("junk")}});
    QVector<Archive::Entry> entries;
    QString reason;
    QVERIFY2(Archive::readZip(zip, entries, reason), qPrintable(reason));
    QCOMPARE(entries.size(), 5);
    const Archive::Entry *hello = findEntry(entries, QStringLiteral("hello.txt"));
    QVERIFY(hello != nullptr && hello->data == QByteArray("hello") && !hello->symlink);
    const Archive::Entry *data = findEntry(entries, QStringLiteral("data.bin"));
    QVERIFY(data != nullptr && data->data == body);
    const Archive::Entry *libfoo = findEntry(entries, QStringLiteral("lib/libfoo.so"));
    QVERIFY(libfoo != nullptr && !libfoo->symlink);
    const Archive::Entry *run = findEntry(entries, QStringLiteral("run.sh"));
    QVERIFY(run != nullptr && run->executable);
    const Archive::Entry *link = findEntry(entries, QStringLiteral("lib/link.so"));
    QVERIFY(link != nullptr && link->symlink && link->data == QByteArray("libfoo.so"));
    // Unpack: files land, links link, exec bits apply.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY2(Archive::unpackTo(entries, dir.path(), reason), qPrintable(reason));
    QVERIFY(QFileInfo(dir.filePath("hello.txt")).isFile());
    QVERIFY(QFileInfo(dir.filePath("lib/libfoo.so")).isFile());
    QVERIFY(QFileInfo(dir.filePath("lib/link.so")).isSymLink());
    QVERIFY(QFileInfo(dir.filePath("run.sh")).isExecutable());
    QVERIFY(!QFileInfo(dir.filePath("__MACOSX/._hello")).exists());
    // Garbage and bad checksums fail loudly, never partially.
    QVERIFY(!Archive::readZip(QByteArray("not a zip"), entries, reason));
    // Flip a payload byte (first file's data starts after its
    // 30-byte local header + 9-byte name): the CRC must catch it.
    QByteArray tampered = zip;
    tampered[39] = static_cast<char>(tampered.at(39) ^ 0xFF);
    QVERIFY(!Archive::readZip(tampered, entries, reason));
}

void TestMainWindow::updaterArchiveTarGz() {
    // Fixture mirrors the linux-static payload: one top-level
    // binary plus a dir and a symlink, gzip-wrapped.
    Archive::Entry bin;
    bin.path = QStringLiteral("bakajiten");
    bin.data = QByteArray(1000, 'y');
    bin.executable = true;
    Archive::Entry share;
    share.path = QStringLiteral("share/");
    Archive::Entry link;
    link.path = QStringLiteral("bakajiten-link");
    link.data = QByteArray("bakajiten");
    link.symlink = true;
    const QByteArray blob = gzipMember(buildTestTar({bin, share, link}));
    QVector<Archive::Entry> entries;
    QString reason;
    QVERIFY2(Archive::readTarGz(blob, entries, reason), qPrintable(reason));
    QCOMPARE(entries.size(), 3);
    const Archive::Entry *found = findEntry(entries, QStringLiteral("bakajiten"));
    QVERIFY(found != nullptr && found->data == bin.data && found->executable);
    const Archive::Entry *foundLink = findEntry(entries, QStringLiteral("bakajiten-link"));
    QVERIFY(foundLink != nullptr && foundLink->symlink);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY2(Archive::unpackTo(entries, dir.path(), reason), qPrintable(reason));
    QVERIFY(QFileInfo(dir.filePath("bakajiten")).isExecutable());
    QVERIFY(QFileInfo(dir.filePath("bakajiten-link")).isSymLink());
    // Traversal and garbage fail loudly.
    Archive::Entry evil;
    evil.path = QStringLiteral("../evil");
    evil.data = QByteArray("x");
    QVERIFY(!Archive::unpackTo({evil}, dir.path(), reason));
    QVERIFY(!Archive::readTarGz(QByteArray("not gzip"), entries, reason));
}

void TestMainWindow::updaterPromptSkip() {
    // Artifact mode offers Download/Later; notice mode is Close
    // only; both carry the per-version opt-out, off by default.
    UpdatePrompt download(QStringLiteral("0.2.0"), QStringLiteral("notes"), true);
    QVERIFY(!download.skipVersion());
    auto *check = download.findChild<QCheckBox *>();
    QVERIFY2(check != nullptr, "skip checkbox missing");
    check->setChecked(true);
    QVERIFY(download.skipVersion());
    auto *buttons = download.findChild<QDialogButtonBox *>();
    QVERIFY2(buttons != nullptr, "button box missing");
    QVERIFY(buttons->button(QDialogButtonBox::Ok) == nullptr);
    bool hasDownload = false;
    for (auto *button : buttons->buttons()) {
        hasDownload = hasDownload || button->text() == QStringLiteral("Download");
    }
    QVERIFY(hasDownload);
    UpdatePrompt notice(QStringLiteral("0.2.0"), QStringLiteral("notes"), false);
    QVERIFY(!notice.skipVersion());
}

void TestMainWindow::updaterProgressFormat() {
    // Human rows for the progress dialog (boundaries included).
    QCOMPARE(UpdateProgress::formatSize(0), QStringLiteral("0 B"));
    QCOMPARE(UpdateProgress::formatSize(512), QStringLiteral("512 B"));
    QCOMPARE(UpdateProgress::formatSize(1024), QStringLiteral("1.0 KB"));
    QCOMPARE(UpdateProgress::formatSize(15360), QStringLiteral("15 KB"));
    QCOMPARE(UpdateProgress::formatSize(5 * 1024 * 1024), QStringLiteral("5.00 MB"));
    QCOMPARE(UpdateProgress::formatSize(48 * 1024 * 1024), QStringLiteral("48.0 MB"));
    QCOMPARE(UpdateProgress::formatSpeed(0.0), QStringLiteral("—"));
    QVERIFY(UpdateProgress::formatSpeed(1500.0).contains(QStringLiteral("/s")));
    QCOMPARE(UpdateProgress::formatEta(0), QStringLiteral("00:00"));
    QCOMPARE(UpdateProgress::formatEta(90), QStringLiteral("01:30"));
    QCOMPARE(UpdateProgress::formatEta(-1), QStringLiteral("—"));
    // Dialog builds modeless with a bar and a cancel path.
    UpdateProgress progress(QStringLiteral("0.2.0"), QStringLiteral("https://example.invalid/x"),
                            QStringLiteral("x.bin"), 100, nullptr);
    QVERIFY(!progress.isModal());
    QVERIFY2(progress.findChild<QProgressBar *>() != nullptr, "bar missing");
}

void TestMainWindow::updaterSkipPersists() {
    // The per-version opt-out round-trips through state.yaml;
    // suite state is shared, so it resets on the way out.
    MemorizedState::setSkippedUpdateVersion(QStringLiteral("0.2.0"));
    QCOMPARE(MemorizedState::skippedUpdateVersion(), QStringLiteral("0.2.0"));
    MemorizedState::setSkippedUpdateVersion(QString());
    QVERIFY(MemorizedState::skippedUpdateVersion().isEmpty());
    // Help menu carries the manual check.
    std::unique_ptr<MainWindow> window(openWindow());
    QVERIFY2(window != nullptr, "window never exposed");
    // App icon (set on QApplication in main.cpp): the embedded
    // resource loads and renders.
    const QIcon appIcon(QStringLiteral(":/icons/app.svg"));
    QVERIFY2(!appIcon.isNull(), "app icon missing");
    QVERIFY2(!appIcon.pixmap(64, 64).isNull(), "app icon never renders");
    QVERIFY2(window->findChild<QAction *>("actionCheckUpdates") != nullptr,
             "check-updates action missing");
}
