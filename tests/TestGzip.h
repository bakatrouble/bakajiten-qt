#pragma once

#include <QObject>

/// Unit tests for the gzip unpacker: roundtrips of Python-gzip
/// members (empty, short, windowed text, FNAME header) plus
/// fail-closed rejects (bad magic/method, truncation, ISIZE
/// mismatch). No network, no fixtures on disk.
class TestGzip : public QObject {
    Q_OBJECT

private slots:
    void unpacksMembers();
    void rejectsCorrupt();
};
