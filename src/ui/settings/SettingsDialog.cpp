#include "ui/settings/SettingsDialog.h"

#include "core/settings/Settings.h"
#include "core/state/SettingsStore.h"
#include "ui_SettingsDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QStyleFactory>

namespace {
// Slider stops at 25% steps within its 75-200 range.
int snapScale(int percent) { return qBound(75, qRound(percent / 25.0) * 25, 200); }

/// Static combo contents (selection applied separately, so the
/// same fill serves construction and post-style rebuilds).
void fillThemeItems(QComboBox *combo) {
    // Theme: OS default first (named, so the source is visible),
    // then every installed style.
    combo->addItem(SettingsDialog::tr("Default (%1)").arg(AppSettings::osStyleName()), QString());
    for (const QString &name : QStyleFactory::keys()) {
        combo->addItem(name, name);
    }
}
void fillModeItems(QComboBox *combo) {
    // Mode: follow the system, or force dark / light (palette
    // applies on change like the theme).
    combo->addItem(SettingsDialog::tr("System"), static_cast<int>(ThemeMode::System));
    combo->addItem(SettingsDialog::tr("Dark"), static_cast<int>(ThemeMode::Dark));
    combo->addItem(SettingsDialog::tr("Light"), static_cast<int>(ThemeMode::Light));
}
void fillReadingItems(QComboBox *combo) {
    // Readings: kana as packed, or romaji in two schools.
    combo->addItem(SettingsDialog::tr("Kana"), static_cast<int>(ReadingDisplay::Kana));
    combo->addItem(SettingsDialog::tr("Rōmaji (Hepburn)"),
                   static_cast<int>(ReadingDisplay::Hepburn));
    combo->addItem(SettingsDialog::tr("Romaji (Kunrei)"), static_cast<int>(ReadingDisplay::Kunrei));
}
void selectByData(QComboBox *combo, const QVariant &data) {
    const QSignalBlocker block(combo);
    const int at = combo->findData(data);
    combo->setCurrentIndex(at < 0 ? 0 : at);
}
} // namespace

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent), ui(new Ui::SettingsDialog) {
    const AppSettings current = AppSettings::fromStorage();
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
        ui->scaleSlider->setValue(snapScale(qRound(current.scale() * 100.0)));
    }
    connect(ui->scaleSlider, &QSlider::valueChanged, this, &SettingsDialog::onScaleChanged);
    // The slider never commits: scale recreates the window, so it
    // stages until OK (the notice tracks the staged value).
    fillThemeItems(ui->themeCombo);
    selectByData(ui->themeCombo, current.style());
    connect(ui->themeCombo, &QComboBox::currentIndexChanged, this, &SettingsDialog::commitSettings);
    fillModeItems(ui->modeCombo);
    selectByData(ui->modeCombo, static_cast<int>(current.mode()));
    connect(ui->modeCombo, &QComboBox::currentIndexChanged, this, &SettingsDialog::commitSettings);
    fillReadingItems(ui->readingCombo);
    selectByData(ui->readingCombo, static_cast<int>(current.reading()));
    connect(ui->readingCombo, &QComboBox::currentIndexChanged, this,
            &SettingsDialog::commitSettings);
    {
        const QSignalBlocker block(ui->checkUpdatesBox);
        ui->checkUpdatesBox->setChecked(current.checkUpdates());
    }
    connect(ui->checkUpdatesBox, &QCheckBox::toggled, this, &SettingsDialog::commitSettings);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    onScaleChanged(ui->scaleSlider->value());
}

SettingsDialog::~SettingsDialog() { delete ui; }

AppSettings SettingsDialog::currentSettings() const {
    AppSettings selected;
    selected.setScale(ui->scaleSlider->value() / 100.0);
    selected.setStyle(ui->themeCombo->currentData().toString());
    selected.setMode(static_cast<ThemeMode>(ui->modeCombo->currentData().toInt()));
    selected.setReading(static_cast<ReadingDisplay>(ui->readingCombo->currentData().toInt()));
    selected.setCheckUpdates(ui->checkUpdatesBox->isChecked());
    return selected;
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
    // Restart notice tracks the staged scale alone (everything else
    // already applied on change). OK arms on a staged scale only:
    // it has nothing to do otherwise.
    const double live = SettingsStore::shared().current().scale();
    const bool staged = !qFuzzyCompare(stepped / 100.0 + 1.0, live + 1.0);
    ui->scaleWarning->setVisible(staged);
    if (QPushButton *ok = ui->buttonBox->button(QDialogButtonBox::Ok)) {
        ok->setEnabled(staged);
    }
}

void SettingsDialog::commitSettings() {
    // A style switch under an open popup leaves the popup
    // container measuring with the old style (wrong item
    // spacing), and popup metrics are born with their combo —
    // so hide any open popup, let the caller apply (which may
    // swap the style and palette), then rebuild combos born
    // under the old style (style untouched: nothing to rebuild).
    for (auto *combo : findChildren<QComboBox *>()) {
        combo->hidePopup();
    }
    // The changing combo keeps focus through the rebuild.
    QString focusName;
    if (auto *senderCombo = qobject_cast<QComboBox *>(sender())) {
        if (senderCombo->window() == this) {
            focusName = senderCombo->objectName();
        }
    }
    const QString before =
        QApplication::style() != nullptr ? QApplication::style()->objectName() : QString();
    emit settingsChanged();
    if (QApplication::style() == nullptr || QApplication::style()->objectName() == before) {
        return;
    }
    rebuildCombo(ui->themeCombo, &fillThemeItems);
    rebuildCombo(ui->modeCombo, &fillModeItems);
    rebuildCombo(ui->readingCombo, &fillReadingItems);
    if (!focusName.isEmpty()) {
        if (QWidget *next = findChild<QComboBox *>(focusName); next != nullptr) {
            next->setFocus(Qt::OtherFocusReason);
        }
    }
}

void SettingsDialog::rebuildCombo(QComboBox *&slot, void (*fill)(QComboBox *)) {
    QComboBox *old = slot;
    const QVariant keep = old->currentData();
    // The combos live in the nested form (the page itself carries
    // the outer vertical layout): look it up by name, not by
    // parent. No form, no rebuild — a stale combo beats none.
    auto *form = findChild<QFormLayout *>("formLayout");
    int row = -1;
    QFormLayout::ItemRole role = QFormLayout::FieldRole;
    if (form != nullptr) {
        form->getWidgetPosition(old, &row, &role);
    }
    if (form == nullptr || row < 0) {
        return;
    }
    auto *fresh = new QComboBox(this);
    fresh->setObjectName(old->objectName());
    fresh->setSizePolicy(old->sizePolicy());
    fresh->setToolTip(old->toolTip());
    // Keep the form position (labels stay where they are).
    form->removeWidget(old);
    fill(fresh);
    selectByData(fresh, keep);
    form->setWidget(row, role, fresh);
    // Layout insertion does not show the widget on its own.
    fresh->show();
    connect(fresh, &QComboBox::currentIndexChanged, this, &SettingsDialog::commitSettings);
    old->hidePopup();
    old->deleteLater();
    slot = fresh;
}
