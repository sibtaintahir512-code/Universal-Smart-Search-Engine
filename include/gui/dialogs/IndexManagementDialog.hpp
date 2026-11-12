#pragma once

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include "gui/workers/IndexingWorker.hpp"

namespace sse::gui {

/**
 * @brief Dialog managing root directory scanning, real-time BFS telemetry, and index rebuilds.
 */
class IndexManagementDialog : public QDialog {
    Q_OBJECT

public:
    explicit IndexManagementDialog(IndexingWorker* worker, QWidget* parent = nullptr);
    ~IndexManagementDialog() override = default;

    QStringList indexedFolders() const;
    void setIndexedFolders(const QStringList& folders);

private slots:
    void onAddFolderClicked();
    void onRemoveFolderClicked();
    void onFullRebuildClicked();
    void onIncrementalSyncClicked();
    void onCancelClicked();

    void onIndexingStarted();
    void onIndexingProgress(quint64 discovered, quint64 indexed, const QString& currentPath);
    void onIndexingCompleted(quint64 discovered, quint64 indexed, double durationSec);
    void onIndexingCancelled();
    void onIndexingFailed(const QString& errorMessage);

private:
    IndexingWorker* m_worker{nullptr};

    QListWidget*  m_folderList{nullptr};
    QPushButton*  m_btnAddFolder{nullptr};
    QPushButton*  m_btnRemoveFolder{nullptr};
    QPushButton*  m_btnFullRebuild{nullptr};
    QPushButton*  m_btnSync{nullptr};
    QPushButton*  m_btnCancel{nullptr};

    QProgressBar* m_progressBar{nullptr};
    QLabel*       m_statusLabel{nullptr};

    void setupUi();
    void setBusy(bool busy);
};

} // namespace sse::gui
