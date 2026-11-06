#include "ui/data/DictionaryManagerDialog.h"

#include "core/data/DataPaths.h"
#include "core/recognizer/ModelPack.h"
#include "core/state/DataStore.h"
#include "dev/UiForm.h"
#include "ui/data/DataChecker.h"
#include "ui/data/DataDownloader.h"
#include "ui/data/DictionaryManagerRow.h"
#include "ui_DictionaryManagerDialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <Qt>
#include <QtConcurrent>

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
        if (title == groups.constFirst().first) {
            // Handwriting pack under Required: Google's Japanese
            // model via dl.google.com (not a release artifact, so
            // no manifest row covers it). Same row widget, same
            // required semantics — no Remove offered.
            handwritingRow = new DictionaryManagerRow(this);
            handwritingRow->setFileName(tr("Japanese handwriting"));
            handwritingRow->setDataFile(QStringLiteral("handwriting"));
            handwritingRow->setAction(tr("Download"), false);
            handwritingRow->setRemoveEnabled(false);
            connect(handwritingRow, &DictionaryManagerRow::downloadRequested, this,
                    &DictionaryManagerDialog::onHandwritingDownload);
            rowsLayout->addWidget(handwritingRow);
            rows.insert(QStringLiteral("handwriting"), handwritingRow);
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
            &DictionaryManagerDialog::onModelFileFinished);
    connect(downloader, &DataDownloader::finished, this,
            &DictionaryManagerDialog::onDownloadFinished);
    connect(downloader, &DataDownloader::failed, this, &DictionaryManagerDialog::onDownloadFailed);
    // Handwriting packs ride a second instance of the same
    // downloader (its own queue/progress; dict jobs keep the
    // manifest write to themselves).
    modelDownloader = new DataDownloader(this);
    connect(modelDownloader, &DataDownloader::progress, this,
            &DictionaryManagerDialog::onDownloadProgress);
    connect(modelDownloader, &DataDownloader::fileFinished, this,
            &DictionaryManagerDialog::onModelFileFinished);
    connect(modelDownloader, &DataDownloader::finished, this,
            &DictionaryManagerDialog::onDownloadFinished);
    connect(modelDownloader, &DataDownloader::failed, this,
            &DictionaryManagerDialog::onDownloadFailed);
    // State lives in the DataStore singleton (last check survives
    // reopening); this view only renders its snapshots.
    DataStore::shared().subscribe(this, [this](const DataStore::Snapshot &) { renderRows(); });
    renderRows();
    onCheck();
}

void DictionaryManagerDialog::onCheck() {
    DataStore::shared().setNotice(tr("Checking for dictionaries…"));
    checker->check();
}

void DictionaryManagerDialog::onChecked(const DataManifest &remoteIn, const QByteArray &bytes) {
    DataStore::shared().setRemote(remoteIn, bytes);
    const QString date = remoteIn.buildDate.isEmpty() ? tr("unknown date") : remoteIn.buildDate;
    DataStore::shared().setNotice(tr("Dictionaries as of %1.").arg(date));
}

void DictionaryManagerDialog::onCheckFailed(const QString &reason) {
    DataStore::shared().setNotice(tr("Update check failed: %1").arg(reason));
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
    DataStore::shared().refresh();
}

void DictionaryManagerDialog::onHandwritingDownload() {
    if (snapshotBusy(QStringLiteral("handwriting"))) {
        return;
    }
    renderHandwritingRow();
    // Same transport as the release artifacts (async, progress in
    // the row); the model has its own downloader instance so the
    // manifest write stays with dict jobs.
    modelDownloader->downloadModel(QStringLiteral("handwriting"), ModelPacks::packs(),
                                   ModelPacks::rootDir());
}

void DictionaryManagerDialog::onModelFileFinished(const QString &file) {
    downloadsLanded = true;
    if (file == QStringLiteral("handwriting")) {
        handwritingLanded = true;
    }
}

bool DictionaryManagerDialog::snapshotBusy(const QString &file) const {
    return DataStore::shared().current().busy.contains(file);
}

void DictionaryManagerDialog::onRequiredAction() {
    downloadFiles(pendingRequired());
    if (!ModelPacks::isCached()) {
        onHandwritingDownload();
    }
}

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
    // Full reset includes the handwriting pack (re-downloaded from
    // the dialog like any required file).
    QDir(ModelPacks::rootDir()).removeRecursively();
    DataStore::shared().refresh();
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void DictionaryManagerDialog::onDownloadProgress(const QString &file, qint64 received,
                                                 qint64 total) {
    const DataStore::Snapshot before = DataStore::shared().current();
    DataStore::shared().setProgress(file, received, total);
    // Transport is the `.gz`: compressed size is the honest total
    // (model packs report their catalog zip sizes instead).
    const qint64 known = total > 0                   ? total
                         : before.remote.has_value() ? before.remote->files.value(file).gz
                                                     : 0;
    const QString label = file == QStringLiteral("handwriting") ? tr("Japanese handwriting")
                                                                : DataPaths::displayName(file);
    DataStore::shared().setNotice(
        tr("Downloading %1… %2 / %3").arg(label, formatMb(received), formatMb(known)));
}

void DictionaryManagerDialog::onDownloadFinished() {
    DataStore::shared().clearBusy();
    // Landed files change on-disk stats: drop the status cache so
    // the next paint re-hashes (rows show Checking…, bulk actions
    // stay disabled, instead of acting on stale verdicts).
    statusesReady = false;
    fileStatuses.clear();
    statusKey.clear();
    QStringList notes;
    if (downloadsLanded) {
        notes << tr("Dictionaries are up to date.");
    }
    if (handwritingLanded) {
        notes << tr("Handwriting model ready.");
    }
    if (!notes.isEmpty()) {
        DataStore::shared().setNotice(notes.join(QStringLiteral(" ")));
    }
    if (downloadsLanded || handwritingLanded) {
        downloadsLanded = false;
        handwritingLanded = false;
        emit dataChanged();
    }
}

void DictionaryManagerDialog::onDownloadFailed(const QString &reason) {
    DataStore::shared().clearBusy();
    DataStore::shared().setNotice(tr("Download failed: %1").arg(reason));
}

/// One hashing pass over the release artifacts (worker thread):
/// per-file freshness against a remote manifest. Inputs by value
/// on purpose (thread boundary — the GUI thread keeps mutating
/// its own copies).
// NOLINTNEXTLINE(performance-unnecessary-value-param)
FileStatusResult checkFileStatuses(DataManifest remote, QStringList files) {
    FileStatusResult result;
    for (const QString &file : files) {
        result.statuses[file] =
            DataManifestReader::status(remote.files.value(file), DataPaths::filePath(file));
    }
    return result;
}

QString DictionaryManagerDialog::statusKeyFor(const DataStore::Snapshot &snapshot) const {
    // Remote bytes (small manifest) plus cheap on-disk stats per
    // file: any new remote, landed download, or removal moves it.
    QString key = QString::number(qHash(snapshot.remoteBytes));
    for (const QString &file : DataPaths::allFiles()) {
        const QFileInfo info(DataPaths::filePath(file));
        key += QStringLiteral(";%1:%2:%3")
                   .arg(file)
                   .arg(info.size())
                   .arg(info.lastModified().toMSecsSinceEpoch());
    }
    return key;
}

void DictionaryManagerDialog::refreshStatuses(const DataStore::Snapshot &snapshot) {
    if (!snapshot.remote.has_value()) {
        return;
    }
    const QString key = statusKeyFor(snapshot);
    if (statusesReady && key == statusKey) {
        return;
    }
    const DataManifest remote = *snapshot.remote;
    const quint64 id = ++statusEpoch;
    auto *watcher = new QFutureWatcher<FileStatusResult>(this);
    connect(watcher, &QFutureWatcher<FileStatusResult>::finished, this,
            [this, watcher]() { onStatusesDone(watcher); });
    watcher->setFuture(QtConcurrent::run([remote, id, key]() {
        FileStatusResult result = checkFileStatuses(remote, DataPaths::allFiles());
        result.epoch = id;
        result.key = key;
        return result;
    }));
}

void DictionaryManagerDialog::onStatusesDone(QFutureWatcher<FileStatusResult> *watcher) {
    const FileStatusResult result = watcher->result();
    watcher->deleteLater();
    if (result.epoch != statusEpoch) {
        // Superseded by a newer pass (landings, removals, re-check).
        return;
    }
    fileStatuses = result.statuses;
    statusKey = result.key;
    statusesReady = true;
    renderRows();
}

QStringList DictionaryManagerDialog::outdatedAll() const {
    // Cached hashing pass (worker thread): empty until the first
    // pass lands; the Update-all button stays disabled meanwhile.
    const std::optional<DataManifest> snapshot = DataStore::shared().current().remote;
    QStringList stale;
    if (!snapshot.has_value() || !statusesReady) {
        return stale;
    }
    // Stale only: missing opt-ins are never auto-fetched.
    for (const QString &file : DataPaths::allFiles()) {
        if (QFileInfo::exists(DataPaths::filePath(file)) &&
            fileStatuses.value(file, DataFileStatus::Missing) == DataFileStatus::Outdated) {
            stale << file;
        }
    }
    return stale;
}

QStringList DictionaryManagerDialog::pendingRequired() const {
    // Same cache rules as outdatedAll().
    const std::optional<DataManifest> snapshot = DataStore::shared().current().remote;
    QStringList pending;
    if (!snapshot.has_value() || !statusesReady) {
        return pending;
    }
    for (const QString &file : DataPaths::requiredFiles()) {
        if (fileStatuses.value(file, DataFileStatus::Missing) != DataFileStatus::Current) {
            pending << file;
        }
    }
    return pending;
}

void DictionaryManagerDialog::downloadFiles(const QStringList &files) {
    const DataStore::Snapshot snapshot = DataStore::shared().current();
    if (!snapshot.remote.has_value() || files.isEmpty()) {
        return;
    }
    downloader->download(*snapshot.remote, snapshot.remoteBytes, files);
}

void DictionaryManagerDialog::renderRows() {
    const DataStore::Snapshot snapshot = DataStore::shared().current();
    infoLabel->setText(snapshot.notice);
    const std::optional<DataManifest> local =
        DataManifestReader::readFile(DataPaths::manifestPath());
    const std::optional<DataManifest> remote = snapshot.remote;
    for (const QString &file : DataPaths::allFiles()) {
        DictionaryManagerRow *row = rows.value(file);
        if (row == nullptr) {
            continue;
        }
        const QString path = DataPaths::filePath(file);
        const bool present = QFileInfo::exists(path);
        const bool required = DataPaths::requiredFiles().contains(file);
        // Progress follows the transfer set (idle rows keep no
        // progress space, shown again by the next signal).
        const bool flying = snapshot.busy.contains(file);
        row->setProgressVisible(flying);
        if (flying) {
            qint64 known = snapshot.total.value(file, 0);
            if (known <= 0 && remote.has_value()) {
                known = remote->files.value(file).gz;
            }
            const qint64 received = snapshot.received.value(file, 0);
            row->setProgress(known > 0 ? static_cast<int>(qMin(received, known) * 1000 / known)
                                       : 0);
        }
        if (!remote.has_value()) {
            row->setStatus(present ? tr("Installed (%1)").arg(formatMb(QFileInfo(path).size()))
                                   : tr("Not downloaded"));
            row->setAction(tr("Download"), false);
            row->setRemoveVisible(present && !required);
            row->setRemoveEnabled(!flying && present && !required);
            continue;
        }
        const DataFileEntry entry = remote->files.value(file);
        if (!remote->files.contains(file)) {
            row->setStatus(tr("No longer published"));
            row->setAction(QStringLiteral("—"), false);
            row->setRemoveVisible(present && !required);
            row->setRemoveEnabled(!flying && present && !required);
            continue;
        }
        const QString localVersion =
            local.has_value() ? local->files.value(file).version : QString();
        if (!statusesReady || !fileStatuses.contains(file)) {
            // Hashing pass still running: rows paint instantly with
            // a placeholder; actions stay disabled until it lands.
            row->setStatus(tr("Checking…"));
            row->setAction(tr("…"), false);
            row->setRemoveVisible(present && !required);
            row->setRemoveEnabled(!flying && present && !required);
            continue;
        }
        switch (fileStatuses.value(file)) {
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
            row->setRemoveEnabled(!flying && present && !required);
            break;
        }
        case DataFileStatus::Current:
            row->setStatus(tr("Up to date%1")
                               .arg(localVersion.isEmpty()
                                        ? QString()
                                        : QStringLiteral(" (%1)").arg(localVersion)));
            row->setAction(tr("Up to date"), false);
            row->setRemoveVisible(present && !required);
            row->setRemoveEnabled(!flying && present && !required);
            break;
        }
        row->setActionEnabled(!flying && !row->isActionComplete());
    }
    const QStringList pending = pendingRequired();
    // Nothing on disk: no updates to check for, nothing to clear.
    bool anyPresent = ModelPacks::isCached();
    for (const QString &file : DataPaths::allFiles()) {
        if (QFileInfo::exists(DataPaths::filePath(file))) {
            anyPresent = true;
            break;
        }
    }
    checkButton->setVisible(anyPresent);
    clearButton->setVisible(anyPresent);
    const bool idle = snapshot.busy.isEmpty();
    const bool handwritingMissing = !ModelPacks::isCached();
    // While the hashing pass runs, bulk buttons stay visible but
    // disabled (no layout motion when the pass lands).
    const bool needRequired = !pending.isEmpty() || handwritingMissing;
    requiredButton->setVisible(remote.has_value() && (statusesReady ? needRequired : true));
    if (!remote.has_value() || (statusesReady && !needRequired)) {
        requiredButton->setEnabled(false);
    } else {
        qint64 total = 0;
        for (const QString &file : pending) {
            total += remote->files.value(file).gz;
        }
        if (handwritingMissing) {
            total += ModelPacks::downloadBytes();
        }
        requiredButton->setText(tr("Download required (%1)").arg(formatMb(total)));
        requiredButton->setEnabled(idle && statusesReady);
    }
    const QStringList stale = outdatedAll();
    updateButton->setVisible(remote.has_value() && (statusesReady ? !stale.isEmpty() : true));
    if (!remote.has_value() || (statusesReady && stale.isEmpty())) {
        updateButton->setEnabled(false);
    } else {
        qint64 total = 0;
        for (const QString &file : stale) {
            total += remote->files.value(file).gz;
        }
        updateButton->setText(tr("Update all (%1)").arg(formatMb(total)));
        updateButton->setEnabled(idle && statusesReady);
    }
    checkButton->setEnabled(idle);
    clearButton->setEnabled(idle);
    renderHandwritingRow();
    refreshStatuses(snapshot);
}

void DictionaryManagerDialog::renderHandwritingRow() {
    if (handwritingRow == nullptr) {
        return;
    }
    const bool cached = ModelPacks::isCached();
    const bool flying = snapshotBusy(QStringLiteral("handwriting"));
    if (flying) {
        handwritingRow->setStatus(tr("Downloading handwriting model…"));
        handwritingRow->setAction(tr("Downloading…"), false);
        handwritingRow->setRemoveVisible(false);
        handwritingRow->setRemoveEnabled(false);
        return;
    }
    if (cached) {
        qint64 bytes = 0;
        QDirIterator it(ModelPacks::rootDir(), QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            bytes += QFileInfo(it.next()).size();
        }
        handwritingRow->setStatus(tr("Downloaded (%1)").arg(formatMb(bytes)));
        handwritingRow->setAction(tr("Up to date"), false);
        // Required model: no individual Remove (like the required
        // dictionaries — a full reset goes through Clear cached
        // files below).
        handwritingRow->setRemoveVisible(false);
        handwritingRow->setRemoveEnabled(false);
        return;
    }
    QStringList lines;
    lines << tr("Not downloaded");
    lines << tr("Japanese model · %1 download").arg(formatMb(ModelPacks::downloadBytes()));
    handwritingRow->setStatus(lines.join(QStringLiteral("\n")));
    handwritingRow->setAction(tr("Download (%1)").arg(formatMb(ModelPacks::downloadBytes())), true);
    handwritingRow->setRemoveVisible(false);
    handwritingRow->setRemoveEnabled(false);
}
