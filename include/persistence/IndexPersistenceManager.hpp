#pragma once

#include <filesystem>
#include <string>
#include <atomic>
#include "persistence/PersistenceResult.hpp"
#include "persistence/PersistenceStatistics.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"

namespace sse::persistence {

/**
 * @brief Central persistence facade coordinating atomic saving, loading,
 * recovery, compaction, and telemetry.
 */
class IndexPersistenceManager {
public:
    IndexPersistenceManager(indexing::InvertedIndex* index,
                            indexing::DocumentCatalog* catalog,
                            std::filesystem::path indexDirectory = "data");

    ~IndexPersistenceManager() = default;

    // Rule of Five (Non-copyable, non-movable due to mutex telemetry and pointer bindings)
    IndexPersistenceManager(const IndexPersistenceManager&) = delete;
    IndexPersistenceManager& operator=(const IndexPersistenceManager&) = delete;
    IndexPersistenceManager(IndexPersistenceManager&&) = delete;
    IndexPersistenceManager& operator=(IndexPersistenceManager&&) = delete;

    /**
     * @brief Saves the current catalog and inverted index to disk atomically.
     *
     * Protocol:
     * 1. Serialize in-memory structures to binary writer.
     * 2. Write to temporary file (search.idx.tmp).
     * 3. Flush and synchronize disk buffers.
     * 4. Verify written file by reading back and checking CRC32.
     * 5. Preserve existing file as backup (search.idx.bak).
     * 6. Atomically replace target file (search.idx).
     * 7. Record telemetry and advance generation.
     */
    PersistenceResult saveIndex(const std::string& customFileName = "");

    /**
     * @brief Loads and restores the search index from disk.
     */
    PersistenceResult loadIndex(const std::string& customFileName = "");

    /**
     * @brief Validates on-disk index file without mutating active memory structures.
     */
    PersistenceResult validateIndex(const std::string& customFileName = "");

    /**
     * @brief Performs crash recovery across primary, backup, and temporary artifacts.
     */
    PersistenceResult recoverIndex(const std::string& customFileName = "");

    /**
     * @brief Compacts active in-memory catalog and index, purging tombstones and remapping DocIDs.
     */
    PersistenceResult compactIndex();

    /**
     * @brief Access cumulative persistence statistics.
     */
    const PersistenceStatistics& statistics() const noexcept { return m_statistics; }
    PersistenceStatistics& statistics() noexcept { return m_statistics; }

    /**
     * @brief Current persistent index generation number.
     */
    uint64_t generation() const noexcept { return m_generation; }

    /**
     * @brief Sets cooperative cancellation token for long-running disk operations.
     */
    void setCancelToken(const std::atomic<bool>* token) noexcept {
        m_cancelToken = token;
    }

    const std::filesystem::path& indexDirectory() const noexcept {
        return m_indexDirectory;
    }

    void setIndexDirectory(std::filesystem::path dir) {
        m_indexDirectory = std::move(dir);
    }

private:
    indexing::InvertedIndex*   m_index{nullptr};
    indexing::DocumentCatalog* m_catalog{nullptr};
    std::filesystem::path      m_indexDirectory;
    PersistenceStatistics      m_statistics;
    uint64_t                   m_generation{1};
    const std::atomic<bool>*   m_cancelToken{nullptr};

    std::filesystem::path resolveFilePath(const std::string& customFileName) const;
};

} // namespace sse::persistence
