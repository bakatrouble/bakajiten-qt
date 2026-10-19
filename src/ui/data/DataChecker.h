#pragma once

#include <QObject>

#include "core/data/DataManifest.h"

class QNetworkAccessManager;

/// Remote manifest fetch: GETs the data repo's latest manifest and
/// reports it. Presentation (missing vs outdated, required vs
/// opt-in) belongs to the caller — this only moves bytes.
class DataChecker : public QObject {
    Q_OBJECT
public:
    explicit DataChecker(QObject *parent = nullptr);

    /// Fetch the latest manifest (one in flight; a second call
    /// while busy is ignored).
    void check();

signals:
    void checked(const DataManifest &remote, const QByteArray &bytes);
    void checkFailed(const QString &reason);

private slots:
    void onReplyFinished();

private:
    QNetworkAccessManager *network = nullptr;
    bool busy = false;
};
