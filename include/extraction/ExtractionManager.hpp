#pragma once

#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <shared_mutex>
#include "core/CommonTypes.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "extraction/IContentExtractor.hpp"
#include "extraction/ExtractionResult.hpp"
#include "extraction/ExtractionCache.hpp"

namespace sse::extraction {

/**
 * @brief Central dispatcher and coordinator for document and multimedia extraction.
 * 
 * Manages:
 * - Extractor registry and polymorphic dispatch by extension & category
 * - Configurable extraction budget limits (Default: 50 MB, Range: 5MB–100MB)
 * - 1 GB safety cutoff (files > 1GB restricted to MetadataOnly)
 * - Memory caching via ExtractionCache
 * - Thread-safe cooperative cancellation
 */
class ExtractionManager {
public:
    static constexpr uint64_t DEFAULT_MAX_BYTES = 50ULL * 1024 * 1024; // 50 MB
    static constexpr uint64_t ABSOLUTE_MAX_CUTOFF = 1024ULL * 1024 * 1024; // 1 GB

    explicit ExtractionManager(uint64_t maxExtractionBytes = DEFAULT_MAX_BYTES);
    ~ExtractionManager() = default;

    // Non-copyable, non-movable (contains std::shared_mutex)
    ExtractionManager(const ExtractionManager&) = delete;
    ExtractionManager& operator=(const ExtractionManager&) = delete;
    ExtractionManager(ExtractionManager&&) noexcept = delete;
    ExtractionManager& operator=(ExtractionManager&&) noexcept = delete;

    /**
     * @brief Registers custom or third-party extractor plugin.
     */
    void registerExtractor(std::shared_ptr<IContentExtractor> extractor);

    /**
     * @brief Sets streaming extraction cap (clamped to 5 MB - 100 MB).
     */
    void setMaxExtractionBytes(uint64_t maxBytes) noexcept;
    uint64_t getMaxExtractionBytes() const noexcept { return m_maxExtractionBytes; }

    /**
     * @brief Finds appropriate extractor for extension and category.
     */
    IContentExtractor* findExtractor(const std::string& extension, FileCategory category) const noexcept;

    /**
     * @brief Primary extraction entry point using standalone path and category.
     */
    ExtractionResult extract(const std::string& filePath,
                            FileCategory category = FileCategory::Other,
                            DocID docId = INVALID_DOC_ID,
                            const std::atomic<bool>* cancelToken = nullptr);

    /**
     * @brief Extraction entry point resolving document via DocumentCatalog.
     */
    ExtractionResult extractDocument(DocID docId,
                                    const indexing::DocumentCatalog& catalog,
                                    const std::atomic<bool>* cancelToken = nullptr);

    ExtractionCache& getCache() noexcept { return m_cache; }
    const ExtractionCache& getCache() const noexcept { return m_cache; }
    void clearCache() noexcept { m_cache.clear(); }

private:
    uint64_t                                       m_maxExtractionBytes{DEFAULT_MAX_BYTES};
    mutable std::shared_mutex                      m_extractorMutex;
    std::vector<std::shared_ptr<IContentExtractor>> m_extractors;
    ExtractionCache                                m_cache;

    void registerDefaultExtractors();
};

} // namespace sse::extraction
