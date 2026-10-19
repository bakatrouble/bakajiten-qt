#pragma once

#include <QByteArray>

#include <optional>

/// gzip member unpacking on the system zlib: header parsed (and
/// stripped) here, raw deflate inflated, ISIZE trailer checked.
/// The static Qt builds use `-system-zlib`, so this is the same
/// copy Qt itself uses — no bundled-second-copy hazard anywhere.
class Gzip {
public:
    /// One gzip member to bytes: ISIZE-checked, nullopt on
    /// malformed input. CRC is not rechecked — the caller verifies
    /// the stronger sha256 against the manifest.
    static std::optional<QByteArray> unpack(const QByteArray &gz);
};
