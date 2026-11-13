#include "gui/adapters/SearchEngineAdapter.hpp"
#include "search/SpellChecker.hpp"
#include <algorithm>
#include <cctype>

namespace sse::gui {

SearchEngineAdapter::SearchEngineAdapter(std::filesystem::path persistenceDir)
    : m_catalog()
    , m_index()
    , m_tokenizer()
    , m_extractionManager()
    , m_pipeline(m_catalog, m_index, m_tokenizer, &m_extractionManager)
    , m_searchService(&m_index, &m_catalog)
    , m_persistenceManager(&m_index, &m_catalog, std::move(persistenceDir))
{
}

search::SearchResponse SearchEngineAdapter::executeSearch(const std::string& query,
                                                         std::size_t offset,
                                                         std::size_t limit,
                                                         ranking::RankingMode mode,
                                                         const std::atomic<bool>* cancelToken) {
    std::shared_lock<std::shared_mutex> lock(m_adapterMutex);
    auto cfg = m_searchService.config();
    cfg.rankingConfig.mode = mode;
    cfg.rankingConfig.enableExplanation = true;
    m_searchService.setConfig(cfg);
    return m_searchService.search(query, offset, limit, cancelToken);
}

bool SearchEngineAdapter::validateQuery(const std::string& query,
                                        std::string& outErrorMessage,
                                        std::size_t& outErrorOffset) const {
    if (query.empty()) {
        outErrorMessage = "Query is empty";
        outErrorOffset = 0;
        return false;
    }

    search::QueryLexer lexer;
    auto lexResult = lexer.tokenize(query);
    if (!lexResult.success) {
        outErrorMessage = lexResult.errorMessage;
        outErrorOffset = lexResult.errorOffset;
        return false;
    }

    search::QueryParser parser;
    auto parseResult = parser.parse(lexResult.tokens);
    if (!parseResult.success) {
        outErrorMessage = parseResult.errorMessage;
        outErrorOffset = parseResult.errorOffset;
        return false;
    }

    outErrorMessage.clear();
    outErrorOffset = 0;
    return true;
}

std::vector<std::string> SearchEngineAdapter::getAutocompleteSuggestions(const std::string& prefix,
                                                                         std::size_t maxSuggestions) const {
    std::vector<std::string> results;
    if (prefix.empty()) {
        return results;
    }

    std::string lowerPrefix = prefix;
    for (char& c : lowerPrefix) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    std::shared_lock<std::shared_mutex> lock(m_adapterMutex);
    auto suggestions = m_index.trie().getTopKSuggestions(lowerPrefix, maxSuggestions);
    results.reserve(suggestions.size());
    for (std::size_t i = 0; i < suggestions.size(); ++i) {
        results.push_back(suggestions[i].term);
    }
    return results;
}

std::vector<std::string> SearchEngineAdapter::getSpellingSuggestions(const std::string& term,
                                                                     std::size_t maxDistance,
                                                                     std::size_t maxSuggestions) const {
    std::vector<std::string> results;
    if (term.empty()) {
        return results;
    }

    std::shared_lock<std::shared_mutex> lock(m_adapterMutex);
    search::SpellChecker checker(&m_index.trie());
    auto suggestions = checker.suggest(term, maxDistance, maxSuggestions);

    results.reserve(suggestions.size());
    for (std::size_t i = 0; i < suggestions.size(); ++i) {
        results.push_back(suggestions[i]);
    }
    return results;
}

extraction::ExtractionResult SearchEngineAdapter::extractPreview(DocID docId,
                                                                 const std::atomic<bool>* cancelToken) {
    std::shared_lock<std::shared_mutex> lock(m_adapterMutex);
    return m_extractionManager.extractDocument(docId, m_catalog, cancelToken);
}

extraction::ExtractionResult SearchEngineAdapter::extractPreviewForPath(const std::string& path,
                                                                        FileCategory category,
                                                                        const std::atomic<bool>* cancelToken) {
    return m_extractionManager.extract(path, category, INVALID_DOC_ID, cancelToken);
}

std::string SearchEngineAdapter::getDocumentPath(DocID docId) const {
    std::shared_lock<std::shared_mutex> lock(m_adapterMutex);
    return m_catalog.getPath(docId);
}

std::string SearchEngineAdapter::getDocumentFilename(DocID docId) const {
    std::shared_lock<std::shared_mutex> lock(m_adapterMutex);
    return m_catalog.getFilename(docId);
}

const FileMetadata* SearchEngineAdapter::getDocumentMetadata(DocID docId) const {
    std::shared_lock<std::shared_mutex> lock(m_adapterMutex);
    return m_catalog.getMetadata(docId);
}

indexing::ScanStatistics SearchEngineAdapter::indexFolders(const std::vector<std::string>& roots,
                                                           bool fullRebuild,
                                                           ProgressCallback progressCb,
                                                           std::atomic<bool>& cancelFlag) {
    std::unique_lock<std::shared_mutex> lock(m_adapterMutex);

    if (fullRebuild) {
        m_index.clear();
        m_catalog.clear();
        m_extractionManager.clearCache();
        m_pipeline.resetStatistics();
    }

    indexing::FileScanner scanner;
    scanner.setRoots(roots);

    uint64_t discoveredCount = 0;
    uint64_t indexedCount = 0;

    auto onFileCallback = [&](const indexing::ExtendedFileRecord& record, FileChangeStatus status) {
        if (cancelFlag.load(std::memory_order_relaxed)) {
            return;
        }
        discoveredCount++;

        // Only index if file was newly added, modified, or on a full rebuild
        if (fullRebuild || status == FileChangeStatus::New || status == FileChangeStatus::Modified) {
            auto res = m_pipeline.indexDocument(record.metadata.docId);
            if (res.success) {
                indexedCount++;
            }
        }

        if (progressCb && (discoveredCount % 25 == 0 || discoveredCount == 1)) {
            progressCb(discoveredCount, indexedCount, record.canonicalPath);
        }
    };

    auto onProgressCallback = [&](const indexing::ScanStatistics& /*stats*/) {
        // Periodic internal scanner updates
    };

    auto scanStats = scanner.scan(m_catalog, onFileCallback, onProgressCallback, cancelFlag);

    if (progressCb) {
        progressCb(discoveredCount, indexedCount, "Completed");
    }

    return scanStats;
}

void SearchEngineAdapter::clearAllData() {
    std::unique_lock<std::shared_mutex> lock(m_adapterMutex);
    m_index.clear();
    m_catalog.clear();
    m_extractionManager.clearCache();
    m_pipeline.resetStatistics();
}

persistence::PersistenceResult SearchEngineAdapter::saveIndex(const std::string& customFileName) {
    std::unique_lock<std::shared_mutex> lock(m_adapterMutex);
    return m_persistenceManager.saveIndex(customFileName);
}

persistence::PersistenceResult SearchEngineAdapter::loadIndex(const std::string& customFileName) {
    std::unique_lock<std::shared_mutex> lock(m_adapterMutex);
    return m_persistenceManager.loadIndex(customFileName);
}

persistence::PersistenceResult SearchEngineAdapter::compactIndex() {
    std::unique_lock<std::shared_mutex> lock(m_adapterMutex);
    return m_persistenceManager.compactIndex();
}

persistence::PersistenceResult SearchEngineAdapter::recoverIndex(const std::string& customFileName) {
    std::unique_lock<std::shared_mutex> lock(m_adapterMutex);
    return m_persistenceManager.recoverIndex(customFileName);
}

persistence::PersistenceResult SearchEngineAdapter::validateIndexFile(const std::string& customFileName) {
    std::shared_lock<std::shared_mutex> lock(m_adapterMutex);
    return m_persistenceManager.validateIndex(customFileName);
}

EngineTelemetry SearchEngineAdapter::getTelemetry() const {
    std::shared_lock<std::shared_mutex> lock(m_adapterMutex);
    EngineTelemetry t;
    t.totalCatalogDocuments = m_catalog.totalCount();
    t.activeCatalogDocuments = m_catalog.activeCount();
    t.vocabularyTerms = m_index.vocabularySize();
    t.trieVocabularyTerms = m_index.trieVocabularySize();
    t.totalPostings = m_index.totalPostingsCount();
    t.persistenceGeneration = m_persistenceManager.generation();
    t.trieNodeCount = m_index.trie().nodeCount();

    const auto& sStats = m_searchService.statistics();
    t.totalQueriesExecuted = sStats.totalRequests();
    t.avgQueryTimeMs = sStats.averageExecutionTimeMs();

    return t;
}

} // namespace sse::gui
