#pragma once

#include <QDialog>
#include <QString>

class QCheckBox;

/// Update notice: what changed and what to do. Artifact mode offers
/// Download (accept) / Later (reject); notice mode (a newer release
/// with no build for this platform) offers Close only. Either way
/// the "do not notify about this version" box is honored by the
/// caller on close.
class UpdatePrompt : public QDialog {
    Q_OBJECT
public:
    /// `hasArtifact` selects Download/Later vs Close-only.
    explicit UpdatePrompt(const QString &version, const QString &notes, bool hasArtifact,
                          QWidget *parent = nullptr);

    /// "Do not notify about this version" checked.
    bool skipVersion() const;

private:
    QCheckBox *skipBox = nullptr;
};
