#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <functional>
#include <shared_mutex>
#include <filesystem>

#include "core/CommonTypes.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include "indexing/Tokenizer.hpp"
#include "indexing/IndexingPipeline.hpp"
#include "indexing/FileScanner.hpp"
#include "extraction/ExtractionManager.hpp"
#include "extraction/ExtractionResult.hpp"
#include "search/SearchService.hpp"
#include "search/SearchResponse.hpp"
#include "search/QueryLexer.hpp"
#include "search/QueryParser.hpp"
#include "ranking/RankingConfig.hpp"
#include "persistence/IndexPersistenceManager.hpp"
#include "persistence/PersistenceResult.hpp"

namespace sse::gui {

/**
 * @brief Telemetry snapshot aggregating statistics across all subsystems.
 */
struct EngineTelemetry {
    std::size_t totalCatalogDocuments{0};
    std::size_t activeCatalogDocuments{0};
    std::size_t vocabularyTerms{0};
    std::size_t trieVocabularyTerms{0};
    uint64_t    totalPostings{0};
    uint64_t    persistenceGeneration{1};
    uint64_t    totalQueriesExecuted{0};
    double      avgQueryTimeMs{0.0};
    double      hashTableLoadFactor{0.0};
    std::size_t trieNodeCount{0};
};

/**
 * @brief Thread-safe adapter facade bridging Qt GUI components to backend core engine.
 */
class SearchEngineAdapter {
public:
    explicit SearchEngineAdapter(std::filesystem::path persistenceDir = "data");
    ~SearchEngineAdapter() = default;

    // Non-copyable, non-movable
    SearchEngineAdapter(const SearchEngineAdapter&) = delete;
    SearchEngineAdapter& operator=(const SearchEngineAdapter&) = delete;
    SearchEngineAdapter(SearchEngineAdapter&&) = delete;
    SearchEngineAdapter& operator=(SearchEngineAdapter&&) = delete;

    // =========================================================================
    // Search & Autocomplete
    // =========================================================================
    search::SearchResponse executeSearch(const std::string& query,
                                         std::size_t offset = 0,
                                         std::size_t limit = 50,
                                         ranking::RankingMode mode = ranking::RankingMode::BM25,
                                         const std::atomic<bool>* cancelToken = nullptr);

    bool validateQuery(const std::string& query,
                       std::string& outErrorMessage,
                       std::size_t& outErrorOffset) const;

    std::vector<std::string> getAutocompleteSuggestions(const std::string& prefix,
                                                         std::size_t maxSuggestions = 5) const;

    std::vector<std::string> getSpellingSuggestions(const std::string& term,
                                                    std::size_t maxDistance = 2,
                                                    std::size_t maxSuggestions = 5) const;

    // =========================================================================
    // Content & Preview Extraction
    // =========================================================================
    extraction::ExtractionResult extractPreview(DocID docId,
                                                const std::atomic<bool>* cancelToken = nullptr);

    extraction::ExtractionResult extractPreviewForPath(const std::string& path,
                                                       FileCategory category,
                                                       const std::atomic<bool>* cancelToken = nullptr);

    std::string getDocumentPath(DocID docId) const;
    std::string getDocumentFilename(DocID docId) const;
    const FileMetadata* getDocumentMetadata(DocID docId) const;

    // =========================================================================
    // Indexing & Scanning
    // =========================================================================
    using ProgressCallback = std::function<void(uint64_t discovered, uint64_t indexed, const std::string& currentPath)>;

    indexing::ScanStatistics indexFolders(const std::vector<std::string>& roots,
                                          bool fullRebuild,
                                          ProgressCallback progressCb,
                                          std::atomic<bool>& cancelFlag);

    void clearAllData();

    // =========================================================================
    // Persistence
    // =========================================================================
    persistence::PersistenceResult saveIndex(const std::string& customFileName = "");
    persistence::PersistenceResult loadIndex(const std::string& customFileName = "");
    persistence::PersistenceResult compactIndex();
    persistence::PersistenceResult recoverIndex(const std::string& customFileName = "");
    persistence::PersistenceResult validateIndexFile(const std::string& customFileName = "");

    // =========================================================================
    // Subsystem Telemetry
    // =========================================================================
    EngineTelemetry getTelemetry() const;

    // Direct read-only accessors
    const indexing::DocumentCatalog& catalog() const noexcept { return m_catalog; }
    const indexing::InvertedIndex& index() const noexcept { return m_index; }
    const extraction::ExtractionManager& extractionManager() const noexcept { return m_extractionManager; }
    const search::SearchService& searchService() const noexcept { return m_searchService; }
    const persistence::IndexPersistenceManager& persistenceManager() const noexcept { return m_persistenceManager; }

private:
    mutable std::shared_mutex               m_adapterMutex;
    indexing::DocumentCatalog               m_catalog;
    indexing::InvertedIndex                 m_index;
    indexing::Tokenizer                     m_tokenizer;
    extraction::ExtractionManager           m_extractionManager;
    indexing::IndexingPipeline              m_pipeline;
    search::SearchService                   m_searchService;
    persistence::IndexPersistenceManager    m_persistenceManager;
};

} // namespace sse::gui
