#include "core/data/Gzip.h"

#include <zlib.h>

namespace {
/// gzip header: magic, method 8, flags, then optional fields.
constexpr int fixedHeader = 10;
/// Skip past the header; nullopt on bad magic/method/flags.
const char *skipHeader(const QByteArray &gz) {
    if (gz.size() < fixedHeader) {
        return nullptr;
    }
    const auto *bytes = reinterpret_cast<const unsigned char *>(gz.constData());
    if (bytes[0] != 0x1f || bytes[1] != 0x8b || bytes[2] != 0x08) {
        return nullptr;
    }
    const unsigned char flags = bytes[3];
    if ((flags & 0xe0) != 0) {
        return nullptr; // reserved bits: not a plain member
    }
    qsizetype at = fixedHeader;
    if ((flags & 0x04) != 0) { // FEXTRA
        if (at + 2 > gz.size()) {
            return nullptr;
        }
        at += 2 + (static_cast<unsigned char>(gz.at(at)) |
                   (static_cast<unsigned char>(gz.at(at + 1)) << 8));
    }
    if ((flags & 0x08) != 0) { // FNAME, NUL-terminated
        const qsizetype end = gz.indexOf('\0', at);
        if (end < 0) {
            return nullptr;
        }
        at = end + 1;
    }
    if ((flags & 0x10) != 0) { // FCOMMENT, NUL-terminated
        const qsizetype end = gz.indexOf('\0', at);
        if (end < 0) {
            return nullptr;
        }
        at = end + 1;
    }
    if ((flags & 0x02) != 0) { // FHCRC
        at += 2;
    }
    if (at + 8 >= gz.size()) {
        return nullptr; // no room for a stream + trailer
    }
    return gz.constData() + at;
}
} // namespace

std::optional<QByteArray> Gzip::unpack(const QByteArray &gz) {
    const char *body = skipHeader(gz);
    if (body == nullptr) {
        return std::nullopt;
    }
    // ISIZE trailer (last 4 bytes, LE): exact expected size, so a
    // truncated stream can never pass as complete.
    const auto *tail = reinterpret_cast<const unsigned char *>(gz.constData() + gz.size() - 4);
    quint32 expected = 0;
    for (int i = 3; i >= 0; --i) {
        expected = (expected << 8) | tail[i];
    }
    z_stream strm = {};
    // -15: raw deflate, no wrapper — the header was parsed (and
    // stripped) above; the ISIZE trailer below is the end check.
    if (inflateInit2(&strm, -15) != Z_OK) {
        return std::nullopt;
    }
    QByteArray out;
    out.reserve(static_cast<qsizetype>(qMin(expected, 1u << 28)));
    strm.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(body));
    // Bound the stream to this member (excludes the 8-byte
    // trailer): inflate must never read into it.
    strm.avail_in = static_cast<uInt>(gz.constData() + gz.size() - 8 - body);
    unsigned char chunk[1 << 16];
    int ret = Z_OK;
    while (ret == Z_OK) {
        strm.next_out = chunk;
        strm.avail_out = sizeof(chunk);
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR ||
            ret == Z_BUF_ERROR) {
            inflateEnd(&strm);
            return std::nullopt;
        }
        out.append(reinterpret_cast<const char *>(chunk), sizeof(chunk) - strm.avail_out);
    }
    inflateEnd(&strm);
    if (ret != Z_STREAM_END || static_cast<quint32>(out.size()) != expected) {
        return std::nullopt;
    }
    return out;
}
