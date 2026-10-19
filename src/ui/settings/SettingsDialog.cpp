#include "ui/settings/SettingsDialog.h"

#include "core/settings/Settings.h"
#include "ui_SettingsDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QStyleFactory>

namespace {
// Slider stops at 25% steps within its 75-200 range.
int snapScale(int percent) { return qBound(75, qRound(percent / 25.0) * 25, 200); }
} // namespace

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::SettingsDialog), original(AppSettings::fromStorage()) {
    ui->setupUi(this);
    // Fixed width for the widest value, so dragging never reflows
    // the row.
    ui->scaleValue->setMinimumWidth(
        ui->scaleValue->fontMetrics().horizontalAdvance(QStringLiteral("200%")));
    // First row selected: single category today, index sync for free
    // as more are added.
    ui->categoryList->setCurrentRow(0);
    connect(ui->categoryList, &QListWidget::currentRowChanged, ui->settingsStack,
            &QStackedWidget::setCurrentIndex);
    {
        const QSignalBlocker block(ui->scaleSlider);
        ui->scaleSlider->setValue(snapScale(qRound(original.scale() * 100.0)));
    }
    connect(ui->scaleSlider, &QSlider::valueChanged, this, &SettingsDialog::onScaleChanged);
    connect(ui->scaleSlider, &QSlider::valueChanged, this, &SettingsDialog::updateDirty);
    // Theme: OS default first (named, so the source is visible),
    // then every installed style.
    ui->themeCombo->addItem(tr("Default (%1)").arg(AppSettings::osStyleName()), QString());
    for (const QString &name : QStyleFactory::keys()) {
        ui->themeCombo->addItem(name, name);
    }
    {
        const QSignalBlocker block(ui->themeCombo);
        const int at = ui->themeCombo->findData(original.style());
        ui->themeCombo->setCurrentIndex(at < 0 ? 0 : at);
    }
    connect(ui->themeCombo, &QComboBox::currentIndexChanged, this, &SettingsDialog::updateDirty);
    // Icons: automatic first (derived from the theme), then fixed.
    ui->iconsCombo->addItem(tr("Auto"), static_cast<int>(IconScheme::Auto));
    ui->iconsCombo->addItem(tr("Light"), static_cast<int>(IconScheme::Light));
    ui->iconsCombo->addItem(tr("Dark"), static_cast<int>(IconScheme::Dark));
    {
        const QSignalBlocker block(ui->iconsCombo);
        const int at = ui->iconsCombo->findData(static_cast<int>(original.icons()));
        ui->iconsCombo->setCurrentIndex(at < 0 ? 0 : at);
    }
    connect(ui->iconsCombo, &QComboBox::currentIndexChanged, this, &SettingsDialog::updateDirty);
    // Readings: kana as packed, or romaji in two schools.
    ui->readingCombo->addItem(tr("Kana"), static_cast<int>(ReadingDisplay::Kana));
    ui->readingCombo->addItem(tr("Rōmaji (Hepburn)"), static_cast<int>(ReadingDisplay::Hepburn));
    ui->readingCombo->addItem(tr("Romaji (Kunrei)"), static_cast<int>(ReadingDisplay::Kunrei));
    {
        const QSignalBlocker block(ui->readingCombo);
        const int at = ui->readingCombo->findData(static_cast<int>(original.reading()));
        ui->readingCombo->setCurrentIndex(at < 0 ? 0 : at);
    }
    connect(ui->readingCombo, &QComboBox::currentIndexChanged, this, &SettingsDialog::updateDirty);
    {
        const QSignalBlocker block(ui->checkUpdatesBox);
        ui->checkUpdatesBox->setChecked(original.checkUpdates());
    }
    connect(ui->checkUpdatesBox, &QCheckBox::toggled, this, &SettingsDialog::updateDirty);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    if (QPushButton *apply = ui->buttonBox->button(QDialogButtonBox::Apply)) {
        connect(apply, &QPushButton::clicked, this, &SettingsDialog::applyRequested);
    }
    onScaleChanged(ui->scaleSlider->value());
    updateDirty();
}

SettingsDialog::~SettingsDialog() { delete ui; }

double SettingsDialog::selectedScale() const { return ui->scaleSlider->value() / 100.0; }

QString SettingsDialog::selectedStyle() const { return ui->themeCombo->currentData().toString(); }

IconScheme SettingsDialog::selectedIcons() const {
    return static_cast<IconScheme>(ui->iconsCombo->currentData().toInt());
}

ReadingDisplay SettingsDialog::selectedReading() const {
    return static_cast<ReadingDisplay>(ui->readingCombo->currentData().toInt());
}

bool SettingsDialog::selectedCheckUpdates() const { return ui->checkUpdatesBox->isChecked(); }

bool SettingsDialog::isDirty() const {
    AppSettings selected;
    selected.setScale(selectedScale());
    selected.setStyle(selectedStyle());
    selected.setIcons(selectedIcons());
    selected.setReading(selectedReading());
    selected.setCheckUpdates(selectedCheckUpdates());
    return selected != original;
}

void SettingsDialog::markApplied() {
    original = AppSettings::fromStorage();
    updateDirty();
}

void SettingsDialog::onScaleChanged(int percent) {
    // Slider stops at 25% steps only: dragging lands anywhere, so
    // snap to the nearest step (re-setting re-emits once, then the
    // value is stable).
    const int stepped = snapScale(percent);
    if (stepped != percent) {
        ui->scaleSlider->setValue(stepped);
        return;
    }
    ui->scaleValue->setText(tr("%1%").arg(percent));
}

void SettingsDialog::updateDirty() {
    const bool dirty = isDirty();
    if (QPushButton *apply = ui->buttonBox->button(QDialogButtonBox::Apply)) {
        apply->setEnabled(dirty);
    }
    // Only a scale change recreates the window (theme + icons
    // apply live); the notice tracks the scale alone.
    ui->scaleWarning->setVisible(!qFuzzyCompare(selectedScale() + 1.0, original.scale() + 1.0));
}
