#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;

/// GitHub release check for application updates. Fire-and-forget:
/// `check()` GETs the latest release, compares its tag against the
/// running version, and reports. Silent unless something needs a
/// human (up-to-date and failures only surface for manual checks).
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject *parent = nullptr);

    /// Ask GitHub for the latest release. `manual` (Help-menu
    /// checks) also reports up-to-date and failures; automatic
    /// checks stay silent unless an update is actionable. Empty
    /// artifact builds never check at all.
    void check(bool manual = false);

    /// Tag newer than current (`v` prefix tolerated; unparseable
    /// either side reads as not-newer, never as an update).
    static bool isNewer(const QString &current, const QString &tag);
    /// First asset name matching the artifact kind (`appimage` →
    /// `*.AppImage`, `win64` → `*.exe`), or empty.
    static QString matchedAsset(const QStringList &names, const QString &artifact);

signals:
    /// Newer release with a downloadable build for this artifact.
    void updateFound(const QString &version, const QString &notes, const QString &asset,
                     const QString &url, qint64 size);
    /// Newer release, but no build for this artifact.
    void noArtifact(const QString &version, const QString &notes);
    /// Manual check states (silent on automatic checks).
    void upToDate();
    void checkFailed(const QString &reason);

private slots:
    void onReply();

private:
    QNetworkAccessManager *network = nullptr;
    bool manual = false;
};
