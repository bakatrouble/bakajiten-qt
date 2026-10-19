#include "ui/update/UpdatePrompt.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
UpdatePrompt::UpdatePrompt(const QString &version, const QString &notes, bool hasArtifact,
                           QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(hasArtifact ? tr("Update available") : tr("Update notice"));
    setModal(true);
    auto *layout = new QVBoxLayout(this);
    auto *title = new QLabel(
        hasArtifact ? tr("bakajiten %1 is available (you have %2).")
                          .arg(version, QCoreApplication::applicationVersion())
                    : tr("bakajiten %1 is available, but it has no build for this platform yet."),
        this);
    title->setWordWrap(true);
    layout->addWidget(title);
    if (!notes.isEmpty()) {
        auto *body = new QTextBrowser(this);
        body->setPlainText(notes);
        body->setMaximumHeight(220);
        layout->addWidget(body);
    }
    skipBox = new QCheckBox(tr("Do not notify about this version"), this);
    layout->addWidget(skipBox);
    auto *buttons = new QDialogButtonBox(this);
    if (hasArtifact) {
        auto *download = buttons->addButton(tr("Download"), QDialogButtonBox::AcceptRole);
        download->setDefault(true);
        buttons->addButton(tr("Later"), QDialogButtonBox::RejectRole);
    } else {
        buttons->addButton(tr("Close"), QDialogButtonBox::RejectRole);
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

bool UpdatePrompt::skipVersion() const { return skipBox != nullptr && skipBox->isChecked(); }
