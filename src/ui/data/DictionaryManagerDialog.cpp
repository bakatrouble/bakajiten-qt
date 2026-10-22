#include "ui/data/DictionaryManagerDialog.h"

#include "core/data/DataPaths.h"
#include "dev/UiForm.h"
#include "ui/data/DataChecker.h"
#include "ui/data/DataDownloader.h"
#include "ui/data/DictionaryManagerRow.h"
#include "ui_DictionaryManagerDialog.h"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QString formatMb(qint64 bytes) {
    return DictionaryManagerDialog::tr("%1 MB").arg(static_cast<double>(bytes) / 1048576.0, 0, 'f',
                                                    1);
}

QString latestLine(const DataFileEntry &entry) {
    if (entry.size > 0) {
        return DictionaryManagerDialog::tr("Latest %1 \u00b7 %2")
            .arg(entry.version, formatMb(entry.size));
    }
    return DictionaryManagerDialog::tr("Latest %1").arg(entry.version);
}
} // namespace

DictionaryManagerDialog::DictionaryManagerDialog(QWidget *parent) : QDialog(parent) {
    auto [built, form] =
        buildForm<Ui::DictionaryManagerDialog>(this, "ui/data/DictionaryManagerDialog.ui");
    ui = built;
    devForm = form;
    setAttribute(Qt::WA_DeleteOnClose);
    infoLabel = ui->infoLabel;
    rowsLayout = ui->rowsLayout;
    // Required group first, then opt-in (mirrors the Android
    // manager sections). One row widget per artifact: name plus
    // action buttons on top, status lines below, its own progress
    // bar while it downloads.
    const QList<QPair<QString, QStringList>> groups = {
        {tr("Required"), DataPaths::requiredFiles()},
        {tr("Optional"), DataPaths::optionalFiles()},
    };
    for (const auto &[title, files] : groups) {
        auto *header = new QLabel(title, this);
        QFont headerFont = header->font();
        headerFont.setBold(true);
        header->setFont(headerFont);
        rowsLayout->addWidget(header);
        for (const QString &file : files) {
            auto *row = new DictionaryManagerRow(this);
            row->setFileName(DataPaths::displayName(file));
            row->setDataFile(file);
            row->setAction(tr("Download"), false);
            row->setRemoveEnabled(false);
            connect(row, &DictionaryManagerRow::downloadRequested, this,
                    [this, file] { onRowAction(file); });
            connect(row, &DictionaryManagerRow::removeRequested, this,
                    [this, file] { onRemoveAction(file); });
            rowsLayout->addWidget(row);
            rows.insert(file, row);
        }
    }
    rowsLayout->addStretch(1);
    auto *buttons = ui->buttonBox;
    requiredButton = buttons->addButton(tr("Download required"), QDialogButtonBox::ActionRole);
    requiredButton->setEnabled(false);
    connect(requiredButton, &QPushButton::clicked, this,
            &DictionaryManagerDialog::onRequiredAction);
    updateButton = buttons->addButton(tr("Update all"), QDialogButtonBox::ActionRole);
    updateButton->setEnabled(false);
    connect(updateButton, &QPushButton::clicked, this, &DictionaryManagerDialog::onUpdateAllAction);
    checkButton = buttons->addButton(tr("Check for updates"), QDialogButtonBox::ActionRole);
    connect(checkButton, &QPushButton::clicked, this, &DictionaryManagerDialog::onCheck);
    clearButton = buttons->addButton(tr("Clear cached files"), QDialogButtonBox::ActionRole);
    connect(clearButton, &QPushButton::clicked, this, &DictionaryManagerDialog::onClearAllAction);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    checker = new DataChecker(this);
    connect(checker, &DataChecker::checked, this, &DictionaryManagerDialog::onChecked);
    connect(checker, &DataChecker::checkFailed, this, &DictionaryManagerDialog::onCheckFailed);
    downloader = new DataDownloader(this);
    connect(downloader, &DataDownloader::progress, this,
            &DictionaryManagerDialog::onDownloadProgress);
    connect(downloader, &DataDownloader::fileFinished, this,
            [this](const QString &) { downloadsLanded = true; });
    connect(downloader, &DataDownloader::finished, this,
            &DictionaryManagerDialog::onDownloadFinished);
    connect(downloader, &DataDownloader::failed, this, &DictionaryManagerDialog::onDownloadFailed);
    renderRows();
    onCheck();
}

void DictionaryManagerDialog::onCheck() {
    infoLabel->setText(tr("Checking for dictionaries…"));
    checker->check();
}

void DictionaryManagerDialog::onChecked(const DataManifest &remoteIn, const QByteArray &bytes) {
    remote = remoteIn;
    remoteBytes = bytes;
    renderRows();
    const QString date = remoteIn.buildDate.isEmpty() ? tr("unknown date") : remoteIn.buildDate;
    infoLabel->setText(tr("Dictionaries as of %1.").arg(date));
}

void DictionaryManagerDialog::onCheckFailed(const QString &reason) {
    infoLabel->setText(tr("Update check failed: %1").arg(reason));
}

void DictionaryManagerDialog::onRowAction(const QString &file) { downloadFiles({file}); }

void DictionaryManagerDialog::onRemoveAction(const QString &file) {
    // Trash arms a confirmation; only Remove deletes. Required
    // files have no Remove button at all (use Clear cached files
    // below for a full reset).
    const QMessageBox::StandardButton answer =
        QMessageBox::question(this, tr("Remove %1?").arg(DataPaths::displayName(file)),
                              tr("The file downloads again on request."));
    if (answer != QMessageBox::Yes) {
        return;
    }
    QFile::remove(DataPaths::filePath(file));
    renderRows();
}

void DictionaryManagerDialog::onRequiredAction() { downloadFiles(pendingRequired()); }

void DictionaryManagerDialog::onUpdateAllAction() { downloadFiles(outdatedAll()); }

void DictionaryManagerDialog::onClearAllAction() {
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Clear all downloaded files?"), tr("Required files download again on request."));
    if (answer != QMessageBox::Yes) {
        return;
    }
    for (const QString &file : DataPaths::allFiles()) {
        QFile::remove(DataPaths::filePath(file));
    }
    renderRows();
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void DictionaryManagerDialog::onDownloadProgress(const QString &file, qint64 received,
                                                 qint64 total) {
    activeFiles.insert(file);
    refreshBusy();
    DictionaryManagerRow *row = rows.value(file);
    if (row == nullptr) {
        return;
    }
    // Transport is the `.gz`: compressed size is the honest total.
    const qint64 known = total > 0 ? total : remote.has_value() ? remote->files.value(file).gz : 0;
    row->setProgressVisible(true);
    row->setProgress(known > 0 ? static_cast<int>(qMin(received, known) * 1000 / known) : 0);
    infoLabel->setText(tr("Downloading %1… %2 / %3")
                           .arg(DataPaths::displayName(file), formatMb(received), formatMb(known)));
}

void DictionaryManagerDialog::onDownloadFinished() {
    activeFiles.clear();
    refreshBusy();
    renderRows();
    infoLabel->setText(tr("Dictionaries are up to date."));
    if (downloadsLanded) {
        downloadsLanded = false;
        emit dataChanged();
    }
}

void DictionaryManagerDialog::onDownloadFailed(const QString &reason) {
    activeFiles.clear();
    refreshBusy();
    renderRows();
    infoLabel->setText(tr("Download failed: %1").arg(reason));
}

QStringList DictionaryManagerDialog::outdatedAll() const {
    const std::optional<DataManifest> snapshot = remote;
    QStringList stale;
    if (!snapshot.has_value()) {
        return stale;
    }
    // Stale only: missing opt-ins are never auto-fetched.
    for (const QString &file : DataPaths::allFiles()) {
        if (QFileInfo::exists(DataPaths::filePath(file)) &&
            DataManifestReader::status(snapshot->files.value(file), DataPaths::filePath(file)) ==
                DataFileStatus::Outdated) {
            stale << file;
        }
    }
    return stale;
}

QStringList DictionaryManagerDialog::pendingRequired() const {
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

void DictionaryManagerDialog::downloadFiles(const QStringList &files) {
    const std::optional<DataManifest> snapshot = remote;
    if (!snapshot.has_value() || files.isEmpty()) {
        return;
    }
    for (const QString &file : files) {
        activeFiles.insert(file);
    }
    refreshBusy();
    downloader->download(*snapshot, remoteBytes, files);
}

void DictionaryManagerDialog::renderRows() {
    const std::optional<DataManifest> local =
        DataManifestReader::readFile(DataPaths::manifestPath());
    for (const QString &file : DataPaths::allFiles()) {
        DictionaryManagerRow *row = rows.value(file);
        if (row == nullptr) {
            continue;
        }
        const QString path = DataPaths::filePath(file);
        const bool present = QFileInfo::exists(path);
        const bool required = DataPaths::requiredFiles().contains(file);
        // Idle rows keep no progress space (shown again by the
        // next progress signal).
        row->setProgressVisible(false);
        if (!remote.has_value()) {
            row->setStatus(present ? tr("Installed (%1)").arg(formatMb(QFileInfo(path).size()))
                                   : tr("Not downloaded"));
            row->setAction(tr("Download"), false);
            row->setRemoveVisible(present && !required);
            row->setRemoveEnabled(present && !required);
            continue;
        }
        const DataFileEntry entry = remote->files.value(file);
        if (!remote->files.contains(file)) {
            row->setStatus(tr("No longer published"));
            row->setAction(QStringLiteral("—"), false);
            row->setRemoveVisible(present && !required);
            row->setRemoveEnabled(present && !required);
            continue;
        }
        const QString localVersion =
            local.has_value() ? local->files.value(file).version : QString();
        switch (DataManifestReader::status(entry, path)) {
        case DataFileStatus::Missing: {
            QStringList lines;
            lines << tr("Not downloaded");
            lines << latestLine(entry);
            row->setStatus(lines.join(QStringLiteral("\n")));
            row->setAction(
                entry.gz > 0 ? tr("Download (%1)").arg(formatMb(entry.gz)) : tr("Download"), true);
            row->setRemoveVisible(false);
            row->setRemoveEnabled(false);
            break;
        }
        case DataFileStatus::Outdated: {
            // Saved stamp + size, then the latest entry (mirrors
            // the Android manager rows).
            QStringList lines;
            lines << tr("Update available (%1)").arg(entry.version);
            QStringList savedBits;
            if (!localVersion.isEmpty()) {
                savedBits << localVersion;
            }
            if (present) {
                savedBits << formatMb(QFileInfo(path).size());
            }
            if (!savedBits.isEmpty()) {
                lines << tr("Saved %1").arg(savedBits.join(QStringLiteral(" \u00b7 ")));
            }
            lines << latestLine(entry);
            row->setStatus(lines.join(QStringLiteral("\n")));
            row->setAction(entry.gz > 0 ? tr("Update (%1)").arg(formatMb(entry.gz)) : tr("Update"),
                           true);
            row->setRemoveVisible(present && !required);
            row->setRemoveEnabled(present && !required);
            break;
        }
        case DataFileStatus::Current:
            row->setStatus(tr("Up to date%1")
                               .arg(localVersion.isEmpty()
                                        ? QString()
                                        : QStringLiteral(" (%1)").arg(localVersion)));
            row->setAction(tr("Up to date"), false);
            row->setRemoveVisible(present && !required);
            row->setRemoveEnabled(present && !required);
            break;
        }
    }
    const QStringList pending = pendingRequired();
    // Nothing on disk: no updates to check for, nothing to clear.
    bool anyPresent = false;
    for (const QString &file : DataPaths::allFiles()) {
        if (QFileInfo::exists(DataPaths::filePath(file))) {
            anyPresent = true;
            break;
        }
    }
    checkButton->setVisible(anyPresent);
    clearButton->setVisible(anyPresent);
    requiredButton->setVisible(remote.has_value() && !pending.isEmpty());
    if (!remote.has_value() || pending.isEmpty()) {
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
    const QStringList stale = outdatedAll();
    updateButton->setVisible(remote.has_value() && !stale.isEmpty());
    if (!remote.has_value() || stale.isEmpty()) {
        updateActionable = false;
    } else {
        qint64 total = 0;
        for (const QString &file : stale) {
            total += remote->files.value(file).gz;
        }
        updateButton->setText(tr("Update all (%1)").arg(formatMb(total)));
        updateButton->setEnabled(true);
        updateActionable = true;
    }
}

void DictionaryManagerDialog::refreshBusy() {
    const bool any = !activeFiles.isEmpty();
    checkButton->setEnabled(!any);
    requiredButton->setEnabled(!any && requiredActionable);
    updateButton->setEnabled(!any && updateActionable);
    clearButton->setEnabled(!any);
    for (const QString &file : DataPaths::allFiles()) {
        DictionaryManagerRow *row = rows.value(file);
        if (row == nullptr) {
            continue;
        }
        const bool flying = activeFiles.contains(file);
        row->setActionEnabled(!flying && !row->isActionComplete());
        row->setRemoveEnabled(!flying && !DataPaths::requiredFiles().contains(file) &&
                              QFileInfo::exists(DataPaths::filePath(file)));
    }
}
