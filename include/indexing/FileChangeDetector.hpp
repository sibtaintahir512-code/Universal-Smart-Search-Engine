#pragma once

#include <string>
#include <filesystem>
#include "core/CommonTypes.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "dsa/CustomDynamicArray.hpp"

namespace sse::indexing {

/**
 * @brief Subsystem for detecting file lifecycle transitions (New, Modified, Unchanged, Missing).
 * Operates strictly on OS metadata (size + lastModifiedTime) in O(1) time per file.
 */
class FileChangeDetector {
public:
    FileChangeDetector() = default;
    ~FileChangeDetector() = default;

    /**
     * @brief Evaluates single-file change status against catalog.
     * @param catalog Current active document catalog.
     * @param canonicalPath Canonical absolute file path.
     * @param currentSize Current file size on disk.
     * @param currentModTime Current last write time epoch on disk.
     * @param existsOnDisk Whether file currently exists on disk.
     * @return FileChangeStatus enumeration.
     */
    static FileChangeStatus detectChange(const DocumentCatalog& catalog,
                                        const std::string& canonicalPath,
                                        uint64_t currentSize,
                                        uint64_t currentModTime,
                                        bool existsOnDisk) noexcept;

    /**
     * @brief Post-scan reconciliation for completed scans.
     * Identifies cataloged documents that are no longer present on disk and marks them soft-deleted.
     * 
     * INVARIANT: Only invoke on fully completed scans! Must NEVER be called if scan was cancelled.
     * 
     * @param catalog Mutable document catalog to update.
     * @param discoveredPaths Array of all canonical paths discovered in this scan.
     * @return Number of documents marked missing/tombstoned.
     */
    static std::size_t reconcileMissingDocuments(DocumentCatalog& catalog,
                                                 const dsa::CustomDynamicArray<std::string>& discoveredPaths);
};

} // namespace sse::indexing
