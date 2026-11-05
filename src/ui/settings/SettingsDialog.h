#pragma once

#include <QDialog>

#include "core/settings/Settings.h"

namespace Ui {
class SettingsDialog;
}

class QComboBox;

/// Application settings: categories on the left, pages on the
/// right (Appearance: interface scale, widget theme, color mode,
/// reading script; Updates: automatic checks). Layout comes from
/// `SettingsDialog.ui`, always compiled via uic (a transient modal
/// builds fresh on every open, so it always reflects the form).
/// The dialog only reports: theme, mode, readings, and updates
/// apply on change via `settingsChanged` (persisting belongs to
/// the caller); the interface scale is staged — it moves the label
/// and arms the restart notice, and only OK commits it (OK stays
/// disabled until a scale change is staged; the window recreates).
/// Cancel drops a staged scale; already-applied settings stay.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override;

    /// Current control values as settings (what OK would commit).
    AppSettings currentSettings() const;

signals:
    /// Theme / readings / updates changed: the caller persists +
    /// applies live (never a scale change — the slider never
    /// emits; OK carries scale).
    void settingsChanged();

private slots:
    /// Slider label + 25% snapping + restart notice (no commit:
    /// scale commits on OK).
    void onScaleChanged(int percent);
    /// Theme / mode / readings / updates changed: hide popups,
    /// report immediately, and rebuild combos when the style moved
    /// (popup metrics are born with their combo).
    void commitSettings();

private:
    /// Rebuild one appearance combo in place (selection kept):
    /// `fill` re-adds its static items.
    void rebuildCombo(QComboBox *&slot, void (*fill)(QComboBox *));

    Ui::SettingsDialog *ui = nullptr;
};
