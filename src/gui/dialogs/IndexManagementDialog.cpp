#include "gui/dialogs/IndexManagementDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>

namespace sse::gui {

IndexManagementDialog::IndexManagementDialog(IndexingWorker* worker, QWidget* parent)
    : QDialog(parent)
    , m_worker(worker)
{
    setupUi();

    if (m_worker) {
        connect(m_worker, &IndexingWorker::indexingStarted, this, &IndexManagementDialog::onIndexingStarted);
        connect(m_worker, &IndexingWorker::indexingProgress, this, &IndexManagementDialog::onIndexingProgress);
        connect(m_worker, &IndexingWorker::indexingCompleted, this, &IndexManagementDialog::onIndexingCompleted);
        connect(m_worker, &IndexingWorker::indexingCancelled, this, &IndexManagementDialog::onIndexingCancelled);
        connect(m_worker, &IndexingWorker::indexingFailed, this, &IndexManagementDialog::onIndexingFailed);
    }
}

void IndexManagementDialog::setupUi() {
    setWindowTitle("Index Management & Folder Discovery");
    resize(620, 440);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    auto* headerLabel = new QLabel("<b>Indexed Root Directories</b>", this);
    mainLayout->addWidget(headerLabel);

    m_folderList = new QListWidget(this);
    m_folderList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mainLayout->addWidget(m_folderList, 1);

    auto* folderBtnLayout = new QHBoxLayout();
    m_btnAddFolder = new QPushButton("+ Add Folder...", this);
    m_btnAddFolder->setObjectName("secondaryButton");
    m_btnRemoveFolder = new QPushButton("- Remove Selected", this);
    m_btnRemoveFolder->setObjectName("secondaryButton");

    folderBtnLayout->addWidget(m_btnAddFolder);
    folderBtnLayout->addWidget(m_btnRemoveFolder);
    folderBtnLayout->addStretch();
    mainLayout->addLayout(folderBtnLayout);

    // Progress Section
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0); // Indeterminate by default
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel("Ready.", this);
    m_statusLabel->setStyleSheet("color: #9CA3AF;");
    mainLayout->addWidget(m_statusLabel);

    // Actions
    auto* actionLayout = new QHBoxLayout();
    m_btnFullRebuild = new QPushButton("Start Full Rebuild", this);
    m_btnSync = new QPushButton("Incremental Sync", this);
    m_btnCancel = new QPushButton("Cancel Scan", this);
    m_btnCancel->setEnabled(false);

    auto* btnClose = new QPushButton("Close", this);
    btnClose->setObjectName("secondaryButton");

    actionLayout->addWidget(m_btnFullRebuild);
    actionLayout->addWidget(m_btnSync);
    actionLayout->addWidget(m_btnCancel);
    actionLayout->addStretch();
    actionLayout->addWidget(btnClose);
    mainLayout->addLayout(actionLayout);

    connect(m_btnAddFolder, &QPushButton::clicked, this, &IndexManagementDialog::onAddFolderClicked);
    connect(m_btnRemoveFolder, &QPushButton::clicked, this, &IndexManagementDialog::onRemoveFolderClicked);
    connect(m_btnFullRebuild, &QPushButton::clicked, this, &IndexManagementDialog::onFullRebuildClicked);
    connect(m_btnSync, &QPushButton::clicked, this, &IndexManagementDialog::onIncrementalSyncClicked);
    connect(m_btnCancel, &QPushButton::clicked, this, &IndexManagementDialog::onCancelClicked);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
}

QStringList IndexManagementDialog::indexedFolders() const {
    QStringList list;
    for (int i = 0; i < m_folderList->count(); ++i) {
        list.append(m_folderList->item(i)->text());
    }
    return list;
}

void IndexManagementDialog::setIndexedFolders(const QStringList& folders) {
    m_folderList->clear();
    m_folderList->addItems(folders);
}

void IndexManagementDialog::onAddFolderClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory to Index");
    if (!dir.isEmpty()) {
        for (int i = 0; i < m_folderList->count(); ++i) {
            if (m_folderList->item(i)->text() == dir) {
                return; // Already added
            }
        }
        m_folderList->addItem(dir);
    }
}

void IndexManagementDialog::onRemoveFolderClicked() {
    auto selected = m_folderList->selectedItems();
    for (auto* item : selected) {
        delete m_folderList->takeItem(m_folderList->row(item));
    }
}

void IndexManagementDialog::onFullRebuildClicked() {
    QStringList folders = indexedFolders();
    if (folders.isEmpty()) {
        QMessageBox::warning(this, "No Folders", "Please add at least one folder to index.");
        return;
    }

    if (m_worker) {
        setBusy(true);
        m_worker->startIndexing(folders, true);
    }
}

void IndexManagementDialog::onIncrementalSyncClicked() {
    QStringList folders = indexedFolders();
    if (folders.isEmpty()) {
        QMessageBox::warning(this, "No Folders", "Please add at least one folder to index.");
        return;
    }

    if (m_worker) {
        setBusy(true);
        m_worker->startIndexing(folders, false);
    }
}

void IndexManagementDialog::onCancelClicked() {
    if (m_worker) {
        m_statusLabel->setText("Cancelling indexing...");
        m_worker->cancel();
    }
}

void IndexManagementDialog::onIndexingStarted() {
    setBusy(true);
    m_statusLabel->setText("Scanning directories via iterative BFS...");
}

void IndexManagementDialog::onIndexingProgress(quint64 discovered, quint64 indexed, const QString& currentPath) {
    m_statusLabel->setText(QString("Discovered: %1 | Indexed: %2 | Current: %3")
                               .arg(discovered)
                               .arg(indexed)
                               .arg(currentPath));
}

void IndexManagementDialog::onIndexingCompleted(quint64 discovered, quint64 indexed, double durationSec) {
    setBusy(false);
    m_statusLabel->setText(QString("Completed in %1s! Discovered: %2 files, Indexed: %3 files.")
                               .arg(QString::number(durationSec, 'f', 2))
                               .arg(discovered)
                               .arg(indexed));
    QMessageBox::information(this, "Indexing Complete",
                             QString("Successfully indexed %1 files out of %2 discovered in %3 seconds.")
                                 .arg(indexed).arg(discovered).arg(QString::number(durationSec, 'f', 2)));
}

void IndexManagementDialog::onIndexingCancelled() {
    setBusy(false);
    m_statusLabel->setText("Indexing was cancelled by user.");
}

void IndexManagementDialog::onIndexingFailed(const QString& errorMessage) {
    setBusy(false);
    m_statusLabel->setText("Indexing failed.");
    QMessageBox::critical(this, "Indexing Failed", errorMessage);
}

void IndexManagementDialog::setBusy(bool busy) {
    m_btnAddFolder->setEnabled(!busy);
    m_btnRemoveFolder->setEnabled(!busy);
    m_btnFullRebuild->setEnabled(!busy);
    m_btnSync->setEnabled(!busy);
    m_btnCancel->setEnabled(busy);
    m_progressBar->setVisible(busy);
}

} // namespace sse::gui
