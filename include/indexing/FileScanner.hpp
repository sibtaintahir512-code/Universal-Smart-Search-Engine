#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <filesystem>
#include "dsa/CustomQueue.hpp"
#include "dsa/CustomDynamicArray.hpp"
#include "dsa/CustomHashTable.hpp"
#include "indexing/ScanStatistics.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/FileMetadataManager.hpp"
#include "indexing/FileChangeDetector.hpp"

namespace sse::indexing {

/**
 * @brief Scanner configuration options.
 */
struct ScannerConfig {
    bool     followSymlinks{false};          // Default false: prevents directory loop hazards
    uint32_t maxDepth{1000};                 // Max folder recursion depth (prevents infinite runaway)
    bool     detectExclusions{true};         // Skip common system / trash folders
};

/**
 * @brief High-performance iterative BFS directory crawler.
 * 
 * Implements:
 * - Iterative queue-driven traversal via CustomQueue<std::string>
 * - Ancestor root path deduplication
 * - Symlink loop cycle suppression via canonical path hashing
 * - Non-throwing OS error code resilience (EACCES / permission denial handling)
 * - Thread-safe cancellation polling
 * - Indeterminate and counted progress telemetry
 */
class FileScanner {
public:
    using FileCallback = std::function<void(const ExtendedFileRecord& record, FileChangeStatus status)>;
    using ProgressCallback = std::function<void(const ScanStatistics& stats)>;

    explicit FileScanner(ScannerConfig config = ScannerConfig());
    ~FileScanner() = default;

    // Configuration
    void setRoots(const std::vector<std::string>& rootPaths);
    void addRoot(const std::string& rootPath);
    void clearRoots() noexcept;

    void setConfig(const ScannerConfig& config) noexcept { m_config = config; }
    const ScannerConfig& getConfig() const noexcept { return m_config; }

    const ScanStatistics& getStatistics() const noexcept { return m_stats; }

    /**
     * @brief Executes BFS discovery across all configured roots.
     * Blocks calling thread until complete or cancelled. Intended for worker thread execution.
     * 
     * @param catalog Mutable document catalog to populate/update.
     * @param onFile Discovered file callback (optional).
     * @param onProgress Periodic progress reporting callback (optional).
     * @param cancelFlag Atomic cancellation token.
     * @return Completed or cancelled scan statistics snapshot.
     */
    ScanStatistics scan(DocumentCatalog& catalog,
                        FileCallback onFile,
                        ProgressCallback onProgress,
                        std::atomic<bool>& cancelFlag);

    /**
     * @brief Static helper to filter redundant child directories from a list of roots.
     */
    static std::vector<std::string> deduplicateRoots(const std::vector<std::string>& roots);

    /**
     * @brief Checks if a directory path matches known OS exclusion signatures ($Recycle.Bin, etc.).
     */
    static bool isExcludedDirectory(const std::filesystem::path& dirPath) noexcept;

private:
    ScannerConfig            m_config;
    std::vector<std::string> m_rootPaths;
    ScanStatistics           m_stats;
    FileMetadataManager      m_metaManager;
};

} // namespace sse::indexing
