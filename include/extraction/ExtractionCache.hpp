#pragma once

#include <string>
#include <shared_mutex>
#include "dsa/CustomHashTable.hpp"
#include "extraction/ExtractionResult.hpp"

namespace sse::extraction {

/**
 * @brief Thread-safe memory cache of extracted content and metadata.
 * 
 * Enforces composite cache identity:
 * - Canonical Path
 * - File Size
 * - Last Modification Timestamp
 * - Extractor ID
 * - Extractor Version
 * 
 * Invalidation is automatic upon file size/timestamp delta or extractor version bump.
 * Backed by Phase 2 CustomHashTable.
 */
class ExtractionCache {
public:
    struct CacheEntry {
        ExtractionResult result;
        uint64_t         cachedModTime{0};
    };

    ExtractionCache() = default;
    ~ExtractionCache() = default;

    // Non-copyable, non-movable (contains std::shared_mutex)
    ExtractionCache(const ExtractionCache&) = delete;
    ExtractionCache& operator=(const ExtractionCache&) = delete;
    ExtractionCache(ExtractionCache&&) noexcept = delete;
    ExtractionCache& operator=(ExtractionCache&&) noexcept = delete;

    /**
     * @brief Looks up cached extraction result.
     * @return true if valid cache hit found and returned; false on cache miss or invalidation.
     */
    bool get(const std::string& canonicalPath,
             uint64_t currentSize,
             uint64_t currentModTime,
             const std::string& extractorId,
             const std::string& extractorVersion,
             ExtractionResult& outResult) const;

    /**
     * @brief Inserts or updates an extraction result in the cache.
     */
    void put(const ExtractionResult& result, uint64_t modTime);
    void put(const std::string& keyPath, const ExtractionResult& result, uint64_t modTime);

    /**
     * @brief Manually invalidates cached entry for path.
     */
    bool invalidate(const std::string& canonicalPath);

    /**
     * @brief Clears entire cache.
     */
    void clear() noexcept;

    /**
     * @brief Current number of cached records.
     */
    std::size_t size() const noexcept;

private:
    mutable std::shared_mutex                  m_mutex;
    dsa::CustomHashTable<std::string, CacheEntry> m_entries;
};

} // namespace sse::extraction
