#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

/// Minimal extractor for the updater's own payloads: zip (stored +
/// deflated entries, directory/symlink aware, `__MACOSX` skipped)
/// and tar (ustar regular/dir/symlink, pax skipped). Anything
/// outside that envelope is an error, never garbage: the archives
/// are our own packaging outputs, small and fully controlled.
class Archive {
public:
    struct Entry {
        QString path;    // forward-slash relative path, never absolute
        QByteArray data; // file bytes; link target for symlinks
        bool symlink = false;
        bool executable = false; // unix owner-exec bit seen
    };

    static bool readZip(const QByteArray &blob, QVector<Entry> &out, QString &reason);
    static bool readTar(const QByteArray &raw, QVector<Entry> &out, QString &reason);
    static bool readTarGz(const QByteArray &blob, QVector<Entry> &out, QString &reason);

    /// Materialize entries under dir (parents created, symlinks via
    /// QFile::link, exec bits applied). Absolute and `..` paths are
    /// refused even though the readers already filter them.
    static bool unpackTo(const QVector<Entry> &entries, const QString &dir, QString &reason);
};
