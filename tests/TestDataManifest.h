#pragma once

#include <QObject>

/// Unit tests for the manifest reader: parsing plus local-file
/// ranking. Pure logic, no network, no fixtures — temp files only.
class TestDataManifest : public QObject {
    Q_OBJECT

private slots:
    void parseReadsEntries();
    void parseRejectsGarbage();
    void statusRanksLocalFiles();
};
