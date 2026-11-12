#include "gui/MainWindow.hpp"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSettings>

namespace sse::gui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    qRegisterMetaType<sse::search::SearchResponse>("sse::search::SearchResponse");
    qRegisterMetaType<sse::gui::PreviewData>("sse::gui::PreviewData");

    m_adapter = std::make_unique<SearchEngineAdapter>("data");

    setupUi();
    setupMenusAndToolbars();
    setupWorkers();

    // Load initial settings
    QSettings settings("UniversalSearchEngine", "SearchEngineApp");
    m_pageSize = settings.value("search/topK", 50).toInt();
    int themeInt = settings.value("ui/theme", static_cast<int>(ThemeMode::Dark)).toInt();
    ThemeManager::applyTheme(static_cast<ThemeMode>(themeInt));

    updateStatusBarTelemetry();
}

MainWindow::~MainWindow() {
    m_searchWorker->cancel();
    m_indexingWorker->cancel();
    m_previewWorker->cancel();

    m_searchThread.quit();
    m_searchThread.wait();

    m_indexingThread.quit();
    m_indexingThread.wait();

    m_previewThread.quit();
    m_previewThread.wait();

    m_persistenceThread.quit();
    m_persistenceThread.wait();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    m_searchWorker->cancel();
    m_indexingWorker->cancel();
    m_previewWorker->cancel();

    m_searchThread.quit();
    m_searchThread.wait();

    m_indexingThread.quit();
    m_indexingThread.wait();

    m_previewThread.quit();
    m_previewThread.wait();

    m_persistenceThread.quit();
    m_persistenceThread.wait();

    event->accept();
}

void MainWindow::setupUi() {
    setWindowTitle("Universal Smart Search Engine — Release 1");
    resize(1280, 800);

    auto* centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    setCentralWidget(centralWidget);

    auto* rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(8);

    // Main 3-Pane Splitter
    auto* mainSplitter = new QSplitter(Qt::Horizontal, centralWidget);
    mainSplitter->setChildrenCollapsible(false);

    // 1. Left Filter Panel
    m_filterPanel = new FilterPanel(mainSplitter);
    m_filterPanel->setMinimumWidth(220);
    m_filterPanel->setMaximumWidth(320);
    mainSplitter->addWidget(m_filterPanel);

    // 2. Center Viewport (Search + Results Table + Pagination)
    auto* centerContainer = new QWidget(mainSplitter);
    auto* centerLayout = new QVBoxLayout(centerContainer);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(8);

    m_searchBar = new SearchBar(m_adapter.get(), centerContainer);
    centerLayout->addWidget(m_searchBar);

    m_summaryLabel = new QLabel("Ready to search. Enter keywords or syntax above.", centerContainer);
    m_summaryLabel->setStyleSheet("color: #9CA3AF; font-size: 12px;");
    centerLayout->addWidget(m_summaryLabel);

    m_resultsView = new QTableView(centerContainer);
    m_resultModel = new SearchResultModel(this);
    m_resultDelegate = new SearchResultDelegate(this);

    m_resultsView->setModel(m_resultModel);
    m_resultsView->setItemDelegate(m_resultDelegate);
    m_resultsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resultsView->setSortingEnabled(true);
    m_resultsView->verticalHeader()->setVisible(false);
    m_resultsView->setShowGrid(false);
    m_resultsView->horizontalHeader()->setSectionResizeMode(SearchResultModel::ColCategory, QHeaderView::ResizeToContents);
    m_resultsView->horizontalHeader()->setSectionResizeMode(SearchResultModel::ColFilename, QHeaderView::Interactive);
    m_resultsView->horizontalHeader()->setSectionResizeMode(SearchResultModel::ColScore, QHeaderView::ResizeToContents);
    m_resultsView->horizontalHeader()->setSectionResizeMode(SearchResultModel::ColSize, QHeaderView::ResizeToContents);
    m_resultsView->horizontalHeader()->setSectionResizeMode(SearchResultModel::ColModified, QHeaderView::ResizeToContents);
    m_resultsView->horizontalHeader()->setSectionResizeMode(SearchResultModel::ColPath, QHeaderView::Stretch);
    m_resultsView->setColumnWidth(SearchResultModel::ColFilename, 220);
    centerLayout->addWidget(m_resultsView, 1);

    // Pagination controls
    auto* paginationLayout = new QHBoxLayout();
    m_btnPrevPage = new QPushButton("◀ Previous", centerContainer);
    m_btnPrevPage->setObjectName("secondaryButton");
    m_btnPrevPage->setEnabled(false);

    m_pageLabel = new QLabel("Page 1", centerContainer);
    m_pageLabel->setAlignment(Qt::AlignCenter);

    m_btnNextPage = new QPushButton("Next ▶", centerContainer);
    m_btnNextPage->setObjectName("secondaryButton");
    m_btnNextPage->setEnabled(false);

    paginationLayout->addStretch();
    paginationLayout->addWidget(m_btnPrevPage);
    paginationLayout->addWidget(m_pageLabel);
    paginationLayout->addWidget(m_btnNextPage);
    paginationLayout->addStretch();
    centerLayout->addLayout(paginationLayout);

    mainSplitter->addWidget(centerContainer);

    // 3. Right Viewport (Tabs: Preview & Metadata)
    m_rightTabWidget = new QTabWidget(mainSplitter);
    m_rightTabWidget->setMinimumWidth(320);

    m_previewPanel = new PreviewPanel(m_rightTabWidget);
    m_metadataPanel = new MetadataPanel(m_rightTabWidget);

    m_rightTabWidget->addTab(m_previewPanel, "Preview");
    m_rightTabWidget->addTab(m_metadataPanel, "Properties & Ranking");
    mainSplitter->addWidget(m_rightTabWidget);

    // Set Splitter ratios: Left 20%, Center 50%, Right 30%
    mainSplitter->setSizes({240, 600, 380});
    rootLayout->addWidget(mainSplitter, 1);

    // Setup Status Bar
    auto* sBar = statusBar();
    m_statusStateLabel = new QLabel("Engine Status: Ready", this);
    m_statusDocCountLabel = new QLabel("Indexed Docs: 0", this);
    m_statusVocabCountLabel = new QLabel("Vocabulary Terms: 0", this);
    m_statusPersistenceLabel = new QLabel("Index Gen: #1", this);

    sBar->addWidget(m_statusStateLabel, 1);
    sBar->addPermanentWidget(m_statusDocCountLabel);
    sBar->addPermanentWidget(m_statusVocabCountLabel);
    sBar->addPermanentWidget(m_statusPersistenceLabel);

    // Internal UI Connections
    connect(m_searchBar, &SearchBar::searchRequested, this, &MainWindow::onSearchRequested);
    connect(m_filterPanel, &FilterPanel::filterChanged, this, &MainWindow::onFilterChanged);
    connect(m_resultsView, &QTableView::clicked, this, &MainWindow::onResultRowClicked);
    connect(m_resultsView, &QTableView::doubleClicked, this, &MainWindow::onResultRowDoubleClicked);
    connect(m_btnPrevPage, &QPushButton::clicked, this, &MainWindow::onPrevPageClicked);
    connect(m_btnNextPage, &QPushButton::clicked, this, &MainWindow::onNextPageClicked);
}

void MainWindow::setupMenusAndToolbars() {
    // Menu Bar
    auto* menuBar = this->menuBar();

    // File Menu
    auto* fileMenu = menuBar->addMenu("&File");
    auto* actManage = fileMenu->addAction("Manage Indexed Folders...", this, &MainWindow::onManageFolders);
    actManage->setShortcut(QKeySequence("Ctrl+O"));
    fileMenu->addSeparator();
    auto* actSave = fileMenu->addAction("Save Index to Disk", this, &MainWindow::onSaveIndex);
    actSave->setShortcut(QKeySequence("Ctrl+S"));
    auto* actLoad = fileMenu->addAction("Reload Index from Disk", this, &MainWindow::onLoadIndex);
    actLoad->setShortcut(QKeySequence("Ctrl+R"));
    fileMenu->addAction("Compact Index (Purge Tombstones)", this, &MainWindow::onCompactIndex);
    fileMenu->addAction("Recover Index from Backup", this, &MainWindow::onRecoverIndex);
    fileMenu->addSeparator();
    auto* actExit = fileMenu->addAction("Exit", this, &QWidget::close);
    actExit->setShortcut(QKeySequence::Quit);

    // View Menu
    auto* viewMenu = menuBar->addMenu("&View");
    auto* actFilter = viewMenu->addAction("Filter Sidebar");
    actFilter->setCheckable(true);
    actFilter->setChecked(true);
    connect(actFilter, &QAction::toggled, m_filterPanel, &QWidget::setVisible);

    auto* actPreview = viewMenu->addAction("Preview & Inspector Dock");
    actPreview->setCheckable(true);
    actPreview->setChecked(true);
    connect(actPreview, &QAction::toggled, m_rightTabWidget, &QWidget::setVisible);

    viewMenu->addSeparator();
    auto* actTheme = viewMenu->addAction("Toggle Dark / Light Theme", this, &MainWindow::onToggleTheme);
    actTheme->setShortcut(QKeySequence("Ctrl+T"));

    // Tools Menu
    auto* toolsMenu = menuBar->addMenu("&Tools");
    auto* actDash = toolsMenu->addAction("Engine Telemetry & Health...", this, &MainWindow::onShowDashboard);
    actDash->setShortcut(QKeySequence("Ctrl+D"));
    auto* actSettings = toolsMenu->addAction("Preferences & Settings...", this, &MainWindow::onShowSettings);
    actSettings->setShortcut(QKeySequence("Ctrl+,"));

    // Help Menu
    auto* helpMenu = menuBar->addMenu("&Help");
    helpMenu->addAction("About Universal Smart Search Engine", this, &MainWindow::onAbout);

    // Toolbar
    auto* toolBar = addToolBar("Main Operations");
    toolBar->setMovable(false);

    auto* actTbarIndex = toolBar->addAction("Index Folders");
    connect(actTbarIndex, &QAction::triggered, this, &MainWindow::onManageFolders);

    auto* actTbarSave = toolBar->addAction("Save Index");
    connect(actTbarSave, &QAction::triggered, this, &MainWindow::onSaveIndex);

    auto* actTbarCompact = toolBar->addAction("Compact");
    connect(actTbarCompact, &QAction::triggered, this, &MainWindow::onCompactIndex);

    toolBar->addSeparator();

    auto* actTbarDash = toolBar->addAction("Telemetry");
    connect(actTbarDash, &QAction::triggered, this, &MainWindow::onShowDashboard);

    auto* actTbarTheme = toolBar->addAction("Toggle Theme");
    connect(actTbarTheme, &QAction::triggered, this, &MainWindow::onToggleTheme);

    auto* actTbarSettings = toolBar->addAction("Settings");
    connect(actTbarSettings, &QAction::triggered, this, &MainWindow::onShowSettings);
}

void MainWindow::setupWorkers() {
    // 1. Search Worker
    m_searchWorker = new SearchWorker(m_adapter.get());
    m_searchWorker->moveToThread(&m_searchThread);
    connect(&m_searchThread, &QThread::finished, m_searchWorker, &QObject::deleteLater);
    connect(m_searchWorker, &SearchWorker::searchStarted, this, &MainWindow::onSearchStarted);
    connect(m_searchWorker, &SearchWorker::searchCompleted, this, &MainWindow::onSearchCompleted);
    connect(m_searchWorker, &SearchWorker::searchFailed, this, &MainWindow::onSearchFailed);
    m_searchThread.start();

    // 2. Indexing Worker
    m_indexingWorker = new IndexingWorker(m_adapter.get());
    m_indexingWorker->moveToThread(&m_indexingThread);
    connect(&m_indexingThread, &QThread::finished, m_indexingWorker, &QObject::deleteLater);
    connect(m_indexingWorker, &IndexingWorker::indexingCompleted, this, [this]() {
        updateStatusBarTelemetry();
    });
    m_indexingThread.start();

    // 3. Preview Worker
    m_previewWorker = new PreviewWorker(m_adapter.get());
    m_previewWorker->moveToThread(&m_previewThread);
    connect(&m_previewThread, &QThread::finished, m_previewWorker, &QObject::deleteLater);
    connect(m_previewWorker, &PreviewWorker::previewReady, this, &MainWindow::onPreviewReady);
    connect(m_previewWorker, &PreviewWorker::previewFailed, this, &MainWindow::onPreviewFailed);
    m_previewThread.start();

    // 4. Persistence Worker
    m_persistenceWorker = new PersistenceWorker(m_adapter.get());
    m_persistenceWorker->moveToThread(&m_persistenceThread);
    connect(&m_persistenceThread, &QThread::finished, m_persistenceWorker, &QObject::deleteLater);
    connect(m_persistenceWorker, &PersistenceWorker::persistenceProgress, this, &MainWindow::onPersistenceProgress);
    connect(m_persistenceWorker, &PersistenceWorker::persistenceCompleted, this, &MainWindow::onPersistenceCompleted);
    m_persistenceThread.start();
}

void MainWindow::onSearchRequested(const QString& query) {
    m_activeQuery = query;
    m_currentPage = 0;
    executeCurrentSearch(0);
}

void MainWindow::executeCurrentSearch(int page) {
    if (m_activeQuery.trimmed().isEmpty()) return;

    QSettings settings("UniversalSearchEngine", "SearchEngineApp");
    int rankingModeInt = settings.value("ranking/mode", static_cast<int>(ranking::RankingMode::BM25)).toInt();

    int offset = page * m_pageSize;

    QMetaObject::invokeMethod(m_searchWorker, "performSearch", Qt::QueuedConnection,
                              Q_ARG(QString, m_activeQuery),
                              Q_ARG(int, offset),
                              Q_ARG(int, m_pageSize),
                              Q_ARG(int, rankingModeInt));
}

void MainWindow::onSearchStarted() {
    m_statusStateLabel->setText("Searching inverted index...");
    m_summaryLabel->setText("Executing search query...");
}

void MainWindow::onSearchCompleted(const sse::search::SearchResponse& response) {
    m_totalMatches = response.totalMatches;
    m_resultModel->setSearchResponse(response);

    if (response.status == search::SearchStatus::Success) {
        m_summaryLabel->setText(QString("Found %1 results in %2 ms for \"%3\"")
                                    .arg(response.totalMatches)
                                    .arg(QString::number(response.executionTimeMs, 'f', 2))
                                    .arg(QString::fromStdString(response.query)));
    } else if (response.status == search::SearchStatus::SyntaxError) {
        m_summaryLabel->setText(QString("Syntax Error: %1").arg(QString::fromStdString(response.errorMessage)));
    } else if (response.status == search::SearchStatus::EmptyQuery) {
        m_summaryLabel->setText("Query is empty.");
    } else {
        m_summaryLabel->setText(QString("Search notice: %1").arg(QString::fromStdString(response.errorMessage)));
    }

    // Update pagination
    std::size_t pageSz = static_cast<std::size_t>(std::max(1, m_pageSize));
    int totalPages = static_cast<int>((m_totalMatches + pageSz - 1) / pageSz);
    if (totalPages == 0) totalPages = 1;

    m_pageLabel->setText(QString("Page %1 of %2").arg(m_currentPage + 1).arg(totalPages));
    m_btnPrevPage->setEnabled(m_currentPage > 0);
    m_btnNextPage->setEnabled(m_currentPage + 1 < totalPages);

    m_statusStateLabel->setText(QString("Search finished in %1 ms").arg(QString::number(response.executionTimeMs, 'f', 2)));
    updateStatusBarTelemetry();
}

void MainWindow::onSearchFailed(const QString& errorMessage) {
    m_summaryLabel->setText(QString("Search failed: %1").arg(errorMessage));
    m_statusStateLabel->setText("Search failed.");
    m_resultModel->clear();
}

void MainWindow::onFilterChanged(const ResultFilterCriteria& criteria) {
    m_resultModel->setFilterCriteria(criteria);
    m_summaryLabel->setText(QString("Showing %1 of %2 matching files")
                                .arg(m_resultModel->visibleCount())
                                .arg(m_resultModel->totalUnfilteredCount()));
}

void MainWindow::onResultRowClicked(const QModelIndex& index) {
    if (!index.isValid()) return;

    const auto* item = m_resultModel->getItem(index.row());
    if (!item) return;

    // Load preview via PreviewWorker
    QMetaObject::invokeMethod(m_previewWorker, "loadPreview", Qt::QueuedConnection,
                              Q_ARG(quint32, static_cast<quint32>(item->docId)),
                              Q_ARG(QString, QString::fromStdString(item->path)),
                              Q_ARG(int, static_cast<int>(item->category)),
                              Q_ARG(QString, m_activeQuery));

    // Update metadata panel immediately
    m_metadataPanel->setItem(item, m_adapter.get());
}

void MainWindow::onResultRowDoubleClicked(const QModelIndex& index) {
    if (!index.isValid()) return;

    const auto* item = m_resultModel->getItem(index.row());
    if (item && !item->path.empty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(item->path)));
    }
}

void MainWindow::onPrevPageClicked() {
    if (m_currentPage > 0) {
        m_currentPage--;
        executeCurrentSearch(m_currentPage);
    }
}

void MainWindow::onNextPageClicked() {
    std::size_t pageSz = static_cast<std::size_t>(std::max(1, m_pageSize));
    int totalPages = static_cast<int>((m_totalMatches + pageSz - 1) / pageSz);
    if (m_currentPage + 1 < totalPages) {
        m_currentPage++;
        executeCurrentSearch(m_currentPage);
    }
}

void MainWindow::onPreviewReady(const sse::gui::PreviewData& data) {
    m_previewPanel->setPreviewData(data, m_activeQuery);
}

void MainWindow::onPreviewFailed(quint32 /*docId*/, const QString& errorMessage) {
    PreviewData errData;
    errData.errorMessage = errorMessage;
    m_previewPanel->setPreviewData(errData);
}

void MainWindow::onSaveIndex() {
    QMetaObject::invokeMethod(m_persistenceWorker, "saveIndex", Qt::QueuedConnection,
                              Q_ARG(QString, QString()));
}

void MainWindow::onLoadIndex() {
    QMetaObject::invokeMethod(m_persistenceWorker, "loadIndex", Qt::QueuedConnection,
                              Q_ARG(QString, QString()));
}

void MainWindow::onCompactIndex() {
    QMetaObject::invokeMethod(m_persistenceWorker, "compactIndex", Qt::QueuedConnection);
}

void MainWindow::onRecoverIndex() {
    QMetaObject::invokeMethod(m_persistenceWorker, "recoverIndex", Qt::QueuedConnection,
                              Q_ARG(QString, QString()));
}

void MainWindow::onPersistenceProgress(const QString& stage) {
    m_statusStateLabel->setText(stage);
}

void MainWindow::onPersistenceCompleted(bool success, const QString& message, quint64 /*generation*/, double durationMs) {
    m_statusStateLabel->setText(QString("Persistence: %1 (%2 ms)").arg(message).arg(QString::number(durationMs, 'f', 1)));
    updateStatusBarTelemetry();
    if (!success) {
        QMessageBox::warning(this, "Persistence Notice", message);
    }
}

void MainWindow::onManageFolders() {
    IndexManagementDialog dlg(m_indexingWorker, this);
    QSettings settings("UniversalSearchEngine", "SearchEngineApp");
    QStringList folders = settings.value("indexing/folders").toStringList();
    dlg.setIndexedFolders(folders);

    if (dlg.exec() == QDialog::Accepted) {
        settings.setValue("indexing/folders", dlg.indexedFolders());
    }
}

void MainWindow::onShowDashboard() {
    DashboardDialog dlg(m_adapter.get(), this);
    dlg.exec();
}

void MainWindow::onShowSettings() {
    SettingsDialog dlg(this);
    connect(&dlg, &SettingsDialog::settingsSaved, this, &MainWindow::onSettingsSaved);
    dlg.exec();
}

void MainWindow::onToggleTheme() {
    ThemeManager::toggleTheme();
    QSettings settings("UniversalSearchEngine", "SearchEngineApp");
    settings.setValue("ui/theme", static_cast<int>(ThemeManager::currentTheme()));
}

void MainWindow::onSettingsSaved() {
    QSettings settings("UniversalSearchEngine", "SearchEngineApp");
    m_pageSize = settings.value("search/topK", 50).toInt();
    int themeInt = settings.value("ui/theme", static_cast<int>(ThemeMode::Dark)).toInt();
    ThemeManager::applyTheme(static_cast<ThemeMode>(themeInt));

    if (!m_activeQuery.isEmpty()) {
        executeCurrentSearch(m_currentPage);
    }
}

void MainWindow::onAbout() {
    QMessageBox::about(this, "About Universal Smart Search Engine",
                       "<h3>Universal Smart Search Engine</h3>"
                       "<p>Release 1 — Phase 9 Desktop Application</p>"
                       "<p>Target Platform: Windows 10/11 x64 (MSVC 2022 / C++17 / Qt 6 Widgets)<br>"
                       "Core Architecture: Custom DSA Library (Zero STL containers), "
                       "Modular Extraction, Inverted Index, BM25 / TF-IDF Ranking, and Binary Atomic Persistence.</p>"
                       "<p><b>Author:</b> Sibtain Abbas &lt;sibtaintahir512@gmail.com&gt;</p>");
}

void MainWindow::updateStatusBarTelemetry() {
    if (!m_adapter) return;

    auto t = m_adapter->getTelemetry();
    m_statusDocCountLabel->setText(QString("Indexed Docs: %1").arg(t.activeCatalogDocuments));
    m_statusVocabCountLabel->setText(QString("Vocabulary: %1").arg(t.vocabularyTerms));
    m_statusPersistenceLabel->setText(QString("Index Gen: #%1").arg(t.persistenceGeneration));
}

} // namespace sse::gui
