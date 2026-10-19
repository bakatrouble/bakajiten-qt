#include "ui/data/DataDialog.h"

#include "core/data/DataPaths.h"
#include "ui/data/DataChecker.h"
#include "ui/data/DataDownloader.h"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QString formatMb(qint64 bytes) {
    return DataDialog::tr("%1 MB").arg(static_cast<double>(bytes) / 1048576.0, 0, 'f', 1);
}
} // namespace

DataDialog::DataDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Dictionaries"));
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose);
    auto *layout = new QVBoxLayout(this);
    infoLabel = new QLabel(tr("Checking for dictionaries…"), this);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);
    // Required vs opt-in, stated once: the layout below this never
    // moves (buttons pre-exist greyed out; progress has its own bar).
    auto *notice = new QLabel(
        tr("English, Kanji and Handwriting are required; Names and Kokugo are optional."), this);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    rowsGrid = new QGridLayout();
    int row = 0;
    for (const QString &file : DataPaths::allFiles()) {
        QString name = DataPaths::displayName(file);
        if (DataPaths::isOptional(file)) {
            name += tr(" (optional)");
        }
        rowsGrid->addWidget(new QLabel(name, this), row, 0);
        auto *status = new QLabel(this);
        rowsGrid->addWidget(status, row, 1);
        auto *action = new QPushButton(tr("Download"), this);
        action->setProperty("dataFile", file);
        action->setEnabled(false);
        connect(action, &QPushButton::clicked, this, &DataDialog::onRowAction);
        rowsGrid->addWidget(action, row, 2);
        rows.insert(file, Row{status, action});
        ++row;
    }
    rowsGrid->setColumnStretch(1, 1);
    layout->addLayout(rowsGrid);
    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 1000);
    progressBar->setValue(0);
    progressBar->setTextVisible(true);
    // Only visible while downloading — but the row keeps its space
    // so showing it never moves the layout.
    progressBar->setVisible(false);
    QSizePolicy barPolicy = progressBar->sizePolicy();
    barPolicy.setRetainSizeWhenHidden(true);
    progressBar->setSizePolicy(barPolicy);
    layout->addWidget(progressBar);
    auto *buttons = new QDialogButtonBox(this);
    requiredButton = buttons->addButton(tr("Download required"), QDialogButtonBox::ActionRole);
    requiredButton->setEnabled(false);
    connect(requiredButton, &QPushButton::clicked, this, &DataDialog::onRequiredAction);
    checkButton = buttons->addButton(tr("Check for updates"), QDialogButtonBox::ActionRole);
    connect(checkButton, &QPushButton::clicked, this, &DataDialog::onCheck);
    buttons->addButton(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    layout->addWidget(buttons);
    checker = new DataChecker(this);
    connect(checker, &DataChecker::checked, this, &DataDialog::onChecked);
    connect(checker, &DataChecker::checkFailed, this, &DataDialog::onCheckFailed);
    downloader = new DataDownloader(this);
    connect(downloader, &DataDownloader::progress, this, &DataDialog::onDownloadProgress);
    connect(downloader, &DataDownloader::fileFinished, this,
            [this](const QString &) { downloadsLanded = true; });
    connect(downloader, &DataDownloader::finished, this, &DataDialog::onDownloadFinished);
    connect(downloader, &DataDownloader::failed, this, &DataDialog::onDownloadFailed);
    renderRows();
    onCheck();
}

void DataDialog::onCheck() {
    infoLabel->setText(tr("Checking for dictionaries…"));
    checker->check();
}

void DataDialog::onChecked(const DataManifest &remoteIn, const QByteArray &bytes) {
    remote = remoteIn;
    remoteBytes = bytes;
    renderRows();
    const QString date = remoteIn.buildDate.isEmpty() ? tr("unknown date") : remoteIn.buildDate;
    infoLabel->setText(tr("Dictionaries as of %1.").arg(date));
}

void DataDialog::onCheckFailed(const QString &reason) {
    infoLabel->setText(tr("Update check failed: %1").arg(reason));
}

void DataDialog::onRowAction() {
    auto *button = qobject_cast<QPushButton *>(sender());
    if (button == nullptr) {
        return;
    }
    downloadFiles({button->property("dataFile").toString()});
}

void DataDialog::onRequiredAction() { downloadFiles(pendingRequired()); }

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void DataDialog::onDownloadProgress(const QString &file, qint64 received, qint64 total) {
    activeReceived[file] = received;
    updateProgress();
    qint64 known = total;
    if (known <= 0) {
        const std::optional<DataManifest> snapshot = remote;
        if (snapshot.has_value()) {
            known = snapshot->files.value(file).gz;
        }
    }
    infoLabel->setText(tr("Downloading %1… %2 / %3")
                           .arg(DataPaths::displayName(file), formatMb(received), formatMb(known)));
}

void DataDialog::onDownloadFinished() {
    activeFiles.clear();
    activeReceived.clear();
    progressBar->setValue(progressBar->maximum());
    progressBar->setVisible(false);
    setBusy(false);
    renderRows();
    infoLabel->setText(tr("Dictionaries are up to date."));
    if (downloadsLanded) {
        downloadsLanded = false;
        emit dataChanged();
    }
}

void DataDialog::onDownloadFailed(const QString &reason) {
    activeFiles.clear();
    activeReceived.clear();
    progressBar->setValue(0);
    progressBar->setVisible(false);
    setBusy(false);
    renderRows();
    infoLabel->setText(tr("Download failed: %1").arg(reason));
}

QStringList DataDialog::pendingRequired() const {
    const std::optional<DataManifest> snapshot = remote;
    QStringList pending;
    if (!snapshot.has_value()) {
        return pending;
    }
    for (const QString &file : DataPaths::requiredFiles()) {
        if (DataManifestReader::status(snapshot->files.value(file), DataPaths::filePath(file)) !=
            DataFileStatus::Current) {
            pending << file;
        }
    }
    return pending;
}

void DataDialog::downloadFiles(const QStringList &files) {
    const std::optional<DataManifest> snapshot = remote;
    if (!snapshot.has_value() || files.isEmpty()) {
        return;
    }
    activeFiles = files;
    activeReceived.clear();
    activeTotalBytes = 0;
    for (const QString &file : files) {
        activeTotalBytes += snapshot->files.value(file).gz;
    }
    progressBar->setValue(0);
    progressBar->setVisible(true);
    setBusy(true);
    downloader->download(*snapshot, remoteBytes, files);
}

void DataDialog::renderRows() {
    const std::optional<DataManifest> local =
        DataManifestReader::readFile(DataPaths::manifestPath());
    for (const QString &file : DataPaths::allFiles()) {
        Row row = rows.value(file);
        const QString path = DataPaths::filePath(file);
        const bool present = QFileInfo::exists(path);
        if (!remote.has_value()) {
            row.status->setText(present ? tr("Installed (%1)").arg(formatMb(QFileInfo(path).size()))
                                        : tr("Not downloaded"));
            row.action->setText(tr("Download"));
            row.action->setEnabled(false);
            continue;
        }
        const DataFileEntry entry = remote->files.value(file);
        if (!remote->files.contains(file)) {
            row.status->setText(tr("No longer published"));
            row.action->setText(tr("—"));
            row.action->setEnabled(false);
            continue;
        }
        const QString localVersion =
            local.has_value() ? local->files.value(file).version : QString();
        switch (DataManifestReader::status(entry, path)) {
        case DataFileStatus::Missing:
            row.status->setText(tr("Not downloaded (%1)").arg(formatMb(entry.gz)));
            row.action->setText(tr("Download"));
            row.action->setEnabled(true);
            break;
        case DataFileStatus::Outdated:
            row.status->setText(tr("Update available (%1)").arg(entry.version));
            row.action->setText(tr("Update"));
            row.action->setEnabled(true);
            break;
        case DataFileStatus::Current:
            row.status->setText(tr("Up to date%1")
                                    .arg(localVersion.isEmpty()
                                             ? QString()
                                             : QStringLiteral(" (%1)").arg(localVersion)));
            row.action->setText(QStringLiteral("✓"));
            row.action->setEnabled(false);
            break;
        }
    }
    const QStringList pending = pendingRequired();
    if (!remote.has_value()) {
        requiredButton->setText(tr("Download required"));
        requiredButton->setEnabled(false);
        requiredActionable = false;
    } else if (pending.isEmpty()) {
        requiredButton->setText(tr("Required up to date"));
        requiredButton->setEnabled(false);
        requiredActionable = false;
    } else {
        qint64 total = 0;
        for (const QString &file : pending) {
            total += remote->files.value(file).gz;
        }
        requiredButton->setText(tr("Download required (%1)").arg(formatMb(total)));
        requiredButton->setEnabled(true);
        requiredActionable = true;
    }
}

void DataDialog::setBusy(bool busy) {
    checkButton->setEnabled(!busy);
    requiredButton->setEnabled(!busy && requiredActionable);
    for (const QString &file : DataPaths::allFiles()) {
        if (QPushButton *action = rows.value(file).action) {
            action->setEnabled(!busy && action->text() != QStringLiteral("✓"));
        }
    }
}

void DataDialog::updateProgress() {
    if (activeTotalBytes <= 0) {
        progressBar->setValue(0);
        return;
    }
    qint64 done = 0;
    for (const QString &file : activeFiles) {
        done += activeReceived.value(file, 0);
    }
    progressBar->setValue(static_cast<int>(qMin(done, activeTotalBytes) * 1000 / activeTotalBytes));
}
