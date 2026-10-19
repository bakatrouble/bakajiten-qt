#include "util/Archive.h"

#include "core/data/Gzip.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <zlib.h>

namespace {

bool safeRelativePath(const QString &path) {
    if (path.isEmpty() || QDir::isAbsolutePath(path)) {
        return false;
    }
    for (const QString &part : path.split(QLatin1Char('/'))) {
        if (part.isEmpty() || part == QStringLiteral(".") || part == QStringLiteral("..")) {
            return false;
        }
    }
    return true;
}

bool skipZipName(const QString &name) {
    if (name.endsWith(QLatin1Char('/'))) {
        return true;
    }
    if (name.startsWith(QStringLiteral("__MACOSX/"))) {
        return true;
    }
    const QString base = name.mid(name.lastIndexOf(QLatin1Char('/')) + 1);
    return base.startsWith(QStringLiteral("._"));
}

quint16 u16le(const QByteArray &blob, int at) {
    const auto *p = reinterpret_cast<const quint8 *>(blob.constData() + at);
    const unsigned lo = p[0];
    const unsigned hi = p[1];
    return static_cast<quint16>(lo | (hi << 8u));
}

quint32 u32le(const QByteArray &blob, int at) {
    const auto *p = reinterpret_cast<const quint8 *>(blob.constData() + at);
    const unsigned b0 = p[0];
    const unsigned b1 = p[1];
    const unsigned b2 = p[2];
    const unsigned b3 = p[3];
    return static_cast<quint32>(b0 | (b1 << 8u) | (b2 << 16u) | (b3 << 24u));
}

bool inflateRaw(const QByteArray &in, int expected, QByteArray &out, QString &reason) {
    out.resize(expected);
    z_stream strm = {};
    strm.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(in.constData()));
    strm.avail_in = static_cast<uInt>(in.size());
    strm.next_out = reinterpret_cast<Bytef *>(out.data());
    strm.avail_out = static_cast<uInt>(out.size());
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) {
        reason = QStringLiteral("Cannot start inflate");
        return false;
    }
    const int ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);
    if (ret != Z_STREAM_END) {
        reason = QStringLiteral("Entry data does not inflate");
        return false;
    }
    return true;
}

bool checkCrc(const QByteArray &data, quint32 want, const QString &name, QString &reason) {
    const quint32 got = crc32(0L, reinterpret_cast<const Bytef *>(data.constData()),
                              static_cast<uInt>(data.size()));
    if (got != want) {
        reason = QStringLiteral("Checksum mismatch in %1").arg(name);
        return false;
    }
    return true;
}

qint64 parseOctal(const QByteArray &field, bool &ok) {
    // NUL/space terminated; embedded spaces are tolerated.
    QByteArray digits;
    for (char c : field) {
        if (c == '\0' || c == ' ') {
            break;
        }
        digits.append(c);
    }
    if (digits.isEmpty()) {
        ok = true;
        return 0;
    }
    return digits.toLongLong(&ok, 8);
}

} // namespace

bool Archive::readZip(const QByteArray &blob, QVector<Entry> &out, QString &reason) {
    out.clear();
    // End-of-central-directory: last 64 KiB + 22, first hit wins.
    const int scanFrom = blob.size() > 65557 ? static_cast<int>(blob.size() - 65557) : 0;
    int eocd = -1;
    for (int i = static_cast<int>(blob.size()) - 22; i >= scanFrom; --i) {
        if (blob[i] == 'P' && blob[i + 1] == 'K' && static_cast<quint8>(blob[i + 2]) == 5 &&
            static_cast<quint8>(blob[i + 3]) == 6) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0) {
        reason = QStringLiteral("Not a zip archive");
        return false;
    }
    const quint32 cdSize = u32le(blob, eocd + 12);
    const quint32 cdOffset = u32le(blob, eocd + 16);
    if (cdSize == 0xFFFFFFFFu || cdOffset == 0xFFFFFFFFu) {
        reason = QStringLiteral("Zip64 archives are not supported");
        return false;
    }
    if (u16le(blob, eocd + 4) != 0 || u16le(blob, eocd + 6) != 0) {
        reason = QStringLiteral("Multi-disk archives are not supported");
        return false;
    }
    int at = static_cast<int>(cdOffset);
    const int cdEnd = static_cast<int>(cdOffset + cdSize);
    while (at < cdEnd) {
        if (at + 46 > blob.size() || blob[at] != 'P' || blob[at + 1] != 'K' || blob[at + 2] != 1 ||
            blob[at + 3] != 2) {
            reason = QStringLiteral("Central directory is truncated");
            return false;
        }
        const quint16 flags = u16le(blob, at + 8);
        const quint16 method = u16le(blob, at + 10);
        const quint32 crc = u32le(blob, at + 16);
        const quint32 compSize = u32le(blob, at + 20);
        const quint32 uncompSize = u32le(blob, at + 24);
        const quint16 nameLen = u16le(blob, at + 28);
        const quint16 extraLen = u16le(blob, at + 30);
        const quint16 commentLen = u16le(blob, at + 32);
        const quint32 attrs = u32le(blob, at + 38);
        const quint32 localOffset = u32le(blob, at + 42);
        if (at + 46 + nameLen + extraLen + commentLen > blob.size()) {
            reason = QStringLiteral("Central directory is truncated");
            return false;
        }
        const QByteArray nameBytes = blob.mid(at + 46, nameLen);
        const bool utf8 = (flags & (1u << 11u)) != 0;
        const QString name = utf8 ? QString::fromUtf8(nameBytes) : QString::fromLatin1(nameBytes);
        at += 46 + nameLen + extraLen + commentLen;
        if ((flags & 1u) != 0u) {
            reason = QStringLiteral("Encrypted entries are not supported");
            return false;
        }
        if (method != 0 && method != 8) {
            reason = QStringLiteral("Unsupported compression in %1").arg(name);
            return false;
        }
        if (compSize == 0xFFFFFFFFu || uncompSize == 0xFFFFFFFFu || localOffset == 0xFFFFFFFFu) {
            reason = QStringLiteral("Zip64 archives are not supported");
            return false;
        }
        if (skipZipName(name)) {
            continue;
        }
        if (!safeRelativePath(name)) {
            reason = QStringLiteral("Unsafe path in %1").arg(name);
            return false;
        }
        // Payload from the local header (sizes trusted from central).
        const int local = static_cast<int>(localOffset);
        if (local + 30 > blob.size() || blob[local] != 'P' || blob[local + 1] != 'K' ||
            blob[local + 2] != 3 || blob[local + 3] != 4) {
            reason = QStringLiteral("Local header missing for %1").arg(name);
            return false;
        }
        const int dataAt = local + 30 + u16le(blob, local + 26) + u16le(blob, local + 28);
        if (dataAt + static_cast<int>(compSize) > blob.size()) {
            reason = QStringLiteral("Entry %1 is truncated").arg(name);
            return false;
        }
        Entry entry;
        entry.path = name;
        const QByteArray raw = blob.mid(dataAt, static_cast<int>(compSize));
        if (method == 0) {
            entry.data = raw;
        } else if (!inflateRaw(raw, static_cast<int>(uncompSize), entry.data, reason)) {
            reason = QStringLiteral("%1 in %2").arg(reason, name);
            return false;
        }
        if (!checkCrc(entry.data, crc, name, reason)) {
            return false;
        }
        const quint32 mode = attrs >> 16u;
        entry.executable = (mode & 0100u) != 0;
        // Unix symlink bit (our macOS bundles carry framework links).
        if ((mode & 0170000u) == 0120000u) {
            entry.symlink = true;
            entry.data = entry.data.split('\0').constFirst(); // link target
        }
        out.append(entry);
    }
    return true;
}

bool Archive::readTar(const QByteArray &raw, QVector<Entry> &out, QString &reason) {
    out.clear();
    QString pendingLongName;
    QString pendingLongLink;
    int at = 0;
    while (at + 512 <= raw.size()) {
        const QByteArray head = raw.mid(at, 512);
        if (head[0] == '\0') {
            break; // first zero block ends the archive
        }
        at += 512;
        const QByteArray magic = head.mid(257, 6);
        if (magic != QByteArray("ustar\0", 6) && magic != QByteArray("ustar  ", 6)) {
            reason = QStringLiteral("Not a ustar archive");
            return false;
        }
        bool ok = false;
        const qint64 size = parseOctal(head.mid(124, 12), ok);
        if (!ok || size < 0) {
            reason = QStringLiteral("Bad size field in tar header");
            return false;
        }
        const int blocks = static_cast<int>((size + 511) / 512);
        if (at + blocks * 512 > raw.size()) {
            reason = QStringLiteral("Tar archive is truncated");
            return false;
        }
        const QByteArray payload = raw.mid(at, static_cast<int>(size));
        at += blocks * 512;
        const char kind = head[156];
        const QString prefix = QString::fromLatin1(head.mid(345, 155).split('\0').constFirst());
        QString name = QString::fromLatin1(head.mid(0, 100).split('\0').constFirst());
        if (!prefix.isEmpty()) {
            name = prefix + QLatin1Char('/') + name;
        }
        if (kind == 'L' || kind == 'K') {
            // GNU long name/link: NUL-terminated payload names the next entry.
            const QString value = QString::fromLatin1(payload.split('\0').constFirst());
            if (kind == 'L') {
                pendingLongName = value;
            } else {
                pendingLongLink = value;
            }
            continue;
        }
        if (kind == 'x' || kind == 'g') {
            continue; // pax extended header: nothing ours needs
        }
        if (!pendingLongName.isEmpty()) {
            name = pendingLongName;
            pendingLongName.clear();
        }
        if (name.endsWith(QLatin1Char('/'))) {
            name.chop(1); // directory entries carry a trailing slash
        }
        if (!safeRelativePath(name)) {
            reason = QStringLiteral("Unsafe path in %1").arg(name);
            return false;
        }
        if (kind == '5') {
            Entry entry;
            entry.path = name;
            out.append(entry);
            continue;
        }
        if (kind == '2') {
            QString target = QString::fromLatin1(head.mid(157, 100).split('\0').constFirst());
            if (!pendingLongLink.isEmpty()) {
                target = pendingLongLink;
                pendingLongLink.clear();
            }
            Entry entry;
            entry.path = name;
            entry.data = target.toUtf8();
            entry.symlink = true;
            out.append(entry);
            continue;
        }
        if (kind != '0' && kind != '\0') {
            reason = QStringLiteral("Unsupported tar entry %1").arg(name);
            return false;
        }
        bool modeOk = false;
        const qint64 modeBits = parseOctal(head.mid(100, 8), modeOk);
        if (!modeOk) {
            reason = QStringLiteral("Bad mode field in %1").arg(name);
            return false;
        }
        Entry entry;
        entry.path = name;
        entry.data = payload;
        entry.executable = (static_cast<quint32>(modeBits) & 0100u) != 0u;
        out.append(entry);
    }
    return true;
}

bool Archive::readTarGz(const QByteArray &blob, QVector<Entry> &out, QString &reason) {
    const std::optional<QByteArray> raw = Gzip::unpack(blob);
    if (!raw.has_value()) {
        reason = QStringLiteral("Not a gzip member");
        return false;
    }
    return readTar(*raw, out, reason);
}

bool Archive::unpackTo(const QVector<Entry> &entries, const QString &dir, QString &reason) {
    for (const Entry &entry : entries) {
        if (!safeRelativePath(entry.path)) {
            reason = QStringLiteral("Unsafe path in %1").arg(entry.path);
            return false;
        }
        const QString full = QDir(dir).filePath(entry.path);
        if (entry.path.endsWith(QLatin1Char('/'))) {
            if (!QDir().mkpath(full)) {
                reason = QStringLiteral("Cannot create %1").arg(full);
                return false;
            }
            continue;
        }
        if (!QDir().mkpath(QFileInfo(full).absolutePath())) {
            reason = QStringLiteral("Cannot create %1").arg(full);
            return false;
        }
        if (entry.symlink) {
            QFile::remove(full);
            if (!QFile::link(QString::fromUtf8(entry.data), full)) {
                reason = QStringLiteral("Cannot link %1").arg(full);
                return false;
            }
            continue;
        }
        QFile out(full);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            reason = QStringLiteral("Cannot write %1").arg(full);
            return false;
        }
        if (out.write(entry.data) != entry.data.size()) {
            reason = QStringLiteral("Cannot write %1").arg(full);
            return false;
        }
        out.close();
        if (entry.executable) {
            QFile::setPermissions(full, QFile::permissions(full) | QFile::ExeOwner |
                                            QFile::ExeGroup | QFile::ExeOther);
        }
    }
    return true;
}
