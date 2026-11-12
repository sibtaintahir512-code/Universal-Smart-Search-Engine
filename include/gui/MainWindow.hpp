#pragma once

#include <QMainWindow>
#include <QThread>
#include <QTableView>
#include <QLabel>
#include <QSplitter>
#include <QTabWidget>
#include <QPushButton>

#include "gui/adapters/SearchEngineAdapter.hpp"
#include "gui/models/SearchResultModel.hpp"
#include "gui/widgets/SearchResultDelegate.hpp"
#include "gui/widgets/SearchBar.hpp"
#include "gui/widgets/FilterPanel.hpp"
#include "gui/widgets/PreviewPanel.hpp"
#include "gui/widgets/MetadataPanel.hpp"
#include "gui/workers/SearchWorker.hpp"
#include "gui/workers/IndexingWorker.hpp"
#include "gui/workers/PreviewWorker.hpp"
#include "gui/workers/PersistenceWorker.hpp"
#include "gui/dialogs/IndexManagementDialog.hpp"
#include "gui/dialogs/DashboardDialog.hpp"
#include "gui/dialogs/SettingsDialog.hpp"

namespace sse::gui {

/**
 * @brief Principal desktop application window for the Universal Smart Search Engine.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    // Search workflow
    void onSearchRequested(const QString& query);
    void onSearchStarted();
    void onSearchCompleted(const sse::search::SearchResponse& response);
    void onSearchFailed(const QString& errorMessage);

    // Filter & Pagination
    void onFilterChanged(const ResultFilterCriteria& criteria);
    void onResultRowClicked(const QModelIndex& index);
    void onResultRowDoubleClicked(const QModelIndex& index);
    void onPrevPageClicked();
    void onNextPageClicked();

    // Preview
    void onPreviewReady(const sse::gui::PreviewData& data);
    void onPreviewFailed(quint32 docId, const QString& errorMessage);

    // Persistence
    void onSaveIndex();
    void onLoadIndex();
    void onCompactIndex();
    void onRecoverIndex();
    void onPersistenceProgress(const QString& stage);
    void onPersistenceCompleted(bool success, const QString& message, quint64 generation, double durationMs);

    // Dialogs & Actions
    void onManageFolders();
    void onShowDashboard();
    void onShowSettings();
    void onToggleTheme();
    void onSettingsSaved();
    void onAbout();

private:
    // Core adapter & models
    std::unique_ptr<SearchEngineAdapter> m_adapter;
    SearchResultModel*                   m_resultModel{nullptr};
    SearchResultDelegate*                m_resultDelegate{nullptr};

    // Background Threads & Workers
    QThread                              m_searchThread;
    SearchWorker*                        m_searchWorker{nullptr};
    QThread                              m_indexingThread;
    IndexingWorker*                      m_indexingWorker{nullptr};
    QThread                              m_previewThread;
    PreviewWorker*                       m_previewWorker{nullptr};
    QThread                              m_persistenceThread;
    PersistenceWorker*                   m_persistenceWorker{nullptr};

    // UI Widgets
    SearchBar*                           m_searchBar{nullptr};
    FilterPanel*                         m_filterPanel{nullptr};
    QTableView*                          m_resultsView{nullptr};
    PreviewPanel*                        m_previewPanel{nullptr};
    MetadataPanel*                       m_metadataPanel{nullptr};
    QTabWidget*                          m_rightTabWidget{nullptr};

    // Results Header & Pagination
    QLabel*                              m_summaryLabel{nullptr};
    QPushButton*                         m_btnPrevPage{nullptr};
    QPushButton*                         m_btnNextPage{nullptr};
    QLabel*                              m_pageLabel{nullptr};

    // Status Bar Labels
    QLabel*                              m_statusStateLabel{nullptr};
    QLabel*                              m_statusDocCountLabel{nullptr};
    QLabel*                              m_statusVocabCountLabel{nullptr};
    QLabel*                              m_statusPersistenceLabel{nullptr};

    // State tracking
    QString                              m_activeQuery;
    int                                  m_currentPage{0};
    int                                  m_pageSize{50};
    std::size_t                          m_totalMatches{0};

    void setupUi();
    void setupMenusAndToolbars();
    void setupWorkers();
    void updateStatusBarTelemetry();
    void executeCurrentSearch(int page);
};

} // namespace sse::gui
