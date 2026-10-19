#pragma once

#include <QDialog>

#include "core/settings/Settings.h"

namespace Ui {
class SettingsDialog;
}

/// Application settings: categories on the left, pages on the
/// right (currently only Appearance: interface scale, widget theme,
/// icon colors). Layout comes from `SettingsDialog.ui`, always
/// compiled via uic (a transient modal builds fresh on every open,
/// so it always reflects the form). The dialog only reports: Apply
/// stays disabled until something differs from the persisted values
/// (kept in one `AppSettings` snapshot); persisting + recreating
/// belongs to the caller (MainWindow), for both OK and Apply — after
/// persisting an Apply that stays open, the caller re-snaps the
/// baseline via `markApplied()`.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override;

    /// Slider value as a factor (1.5 = 150%).
    double selectedScale() const;
    /// Theme combo value (empty = OS default, labeled as such).
    QString selectedStyle() const;
    /// Icons combo value (`Auto` = derive from the theme).
    IconScheme selectedIcons() const;
    /// Readings combo value (kana or romaji school).
    ReadingDisplay selectedReading() const;
    /// Updates checkbox state (automatic checks on/off).
    bool selectedCheckUpdates() const;
    /// Anything differs from the persisted snapshot.
    bool isDirty() const;
    /// Applied the pending values (persisted by the caller):
    /// re-snap the baseline so the dialog reads clean until the
    /// next change.
    void markApplied();

signals:
    /// Apply clicked (only enabled with pending changes): persist,
    /// recreating the window on a scale change (like on OK).
    void applyRequested();

private slots:
    void onScaleChanged(int percent);
    /// Enable Apply (and the restart notice) exactly while dirty.
    void updateDirty();

private:
    Ui::SettingsDialog *ui = nullptr;
    /// Persisted values at open.
    AppSettings original;
};
