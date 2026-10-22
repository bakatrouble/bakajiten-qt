#include "ui/data/DictionaryManagerRow.h"

#include "dev/UiForm.h"
#include "ui_DictionaryManagerRow.h"

DictionaryManagerRow::DictionaryManagerRow(QWidget *parent) : QWidget(parent) {
    auto [built, form] =
        buildForm<Ui::DictionaryManagerRow>(this, "ui/data/DictionaryManagerRow.ui");
    ui = built;
    devForm = form;
    connect(ui->rowAction, &QPushButton::clicked, this, &DictionaryManagerRow::downloadRequested);
    connect(ui->rowRemove, &QPushButton::clicked, this, &DictionaryManagerRow::removeRequested);
}

void DictionaryManagerRow::setFileName(const QString &name) {
    file = name;
    ui->rowName->setText(name);
}

QString DictionaryManagerRow::fileName() const { return file; }

void DictionaryManagerRow::setDataFile(const QString &file) { artifact = file; }

QString DictionaryManagerRow::dataFile() const { return artifact; }

void DictionaryManagerRow::setStatus(const QString &text) { ui->rowStatus->setText(text); }

void DictionaryManagerRow::setAction(const QString &text, bool enabled) {
    ui->rowAction->setText(text);
    ui->rowAction->setEnabled(enabled);
}

void DictionaryManagerRow::setActionEnabled(bool enabled) { ui->rowAction->setEnabled(enabled); }

bool DictionaryManagerRow::isActionComplete() const {
    return ui->rowAction->text() == tr("Up to date");
}

void DictionaryManagerRow::setRemoveEnabled(bool enabled) { ui->rowRemove->setEnabled(enabled); }

void DictionaryManagerRow::setRemoveVisible(bool visible) { ui->rowRemove->setVisible(visible); }

void DictionaryManagerRow::setProgressVisible(bool visible) {
    ui->rowProgress->setVisible(visible);
}

void DictionaryManagerRow::setProgress(int permille) { ui->rowProgress->setValue(permille); }
