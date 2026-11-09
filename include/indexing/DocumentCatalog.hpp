#pragma once

#include <cstdint>
#include <string>
#include <shared_mutex>
#include <mutex>
#include "core/CommonTypes.hpp"
#include "dsa/CustomDynamicArray.hpp"
#include "dsa/CustomHashTable.hpp"

namespace sse::indexing {

/**
 * @brief Central catalog storing and managing all discovered file records.
 * 
 * Enforces:
 * - Persistent Identity: Canonical absolute filesystem path.
 * - Dense Runtime Identity: 32-bit sequential DocID (1 .. N).
 * - No-Reuse Policy: DocIDs are monotonic and never recycled within a scanning session.
 * - Tombstoning: Deleted or obsolete files are soft-deleted via META_FLAG_ACTIVE bit.
 * - Memory Efficiency: Compact contiguous string pool to eliminate heap fragmentation.
 * - Zero STL Containers: Core storage backed entirely by Phase 2 Custom DSA.
 */
class DocumentCatalog {
public:
    DocumentCatalog();
    ~DocumentCatalog() = default;

    // Rule of Five (Non-copyable due to mutex synchronization, movable)
    DocumentCatalog(const DocumentCatalog&) = delete;
    DocumentCatalog& operator=(const DocumentCatalog&) = delete;
    DocumentCatalog(DocumentCatalog&& other) noexcept;
    DocumentCatalog& operator=(DocumentCatalog&& other) noexcept;

    /**
     * @brief Ingests or updates a file in the catalog.
     * Thread-safe with write lock.
     * @param metadata Base metadata record (docId and string offsets overwritten by catalog).
     * @param canonicalPath Canonical absolute filesystem path.
     * @param filename Base filename without directory path.
     * @return Assigned or existing DocID.
     */
    DocID registerDocument(FileMetadata metadata,
                           const std::string& canonicalPath,
                           const std::string& filename);

    /**
     * @brief Fast O(1) lookup of FileMetadata by DocID.
     * Returns nullptr if docId is invalid, out of range, or unassigned.
     */
    const FileMetadata* getMetadata(DocID docId) const noexcept;
    FileMetadata* getMutableMetadata(DocID docId) noexcept;

    /**
     * @brief Extracts full canonical path from the compact string pool.
     */
    std::string getPath(DocID docId) const;

    /**
     * @brief Extracts base filename from the compact string pool.
     */
    std::string getFilename(DocID docId) const;

    /**
     * @brief Reverse lookup from canonical path to DocID in O(1) via CustomHashTable.
     * Returns INVALID_DOC_ID (0) if not cataloged.
     */
    DocID findDocIdByPath(const std::string& canonicalPath) const;

    /**
     * @brief Marks a document as tombstoned (inactive).
     * Does NOT recycle the DocID.
     */
    bool markDeleted(DocID docId);

    /**
     * @brief Reactivates an existing tombstoned document.
     */
    bool reactivate(DocID docId);

    /**
     * @brief Checks if DocID corresponds to a valid, active document.
     */
    bool isActive(DocID docId) const noexcept;

    /**
     * @brief Total documents registered in current session (including tombstones).
     */
    std::size_t totalCount() const noexcept;

    /**
     * @brief Count of currently active (non-tombstoned) documents.
     */
    std::size_t activeCount() const noexcept;

    /**
     * @brief Resets catalog to empty state, clearing records, hash table, and string pool.
     */
    void clear() noexcept;

    /**
     * @brief Restores an existing document entry during index deserialization.
     * Preserves exact original DocID, metadata, and string pool contents.
     */
    bool restoreDocument(const FileMetadata& metadata,
                         const std::string& canonicalPath,
                         const std::string& filename,
                         bool isActive);

private:
    mutable std::shared_mutex               m_mutex;
    DocID                                  m_highestDocId{0};
    std::size_t                            m_activeCount{0};

    dsa::CustomDynamicArray<FileMetadata>  m_records;       // 1-based indexing: index = docId - 1
    dsa::CustomDynamicArray<char>          m_stringPool;    // Contiguous byte buffer for strings
    dsa::CustomHashTable<std::string, DocID> m_pathToDocId; // Canonical path -> DocID

    // Internal helper: appends a string to the string pool and returns offset + length
    void appendToStringPool(const std::string& str, uint32_t& outOffset, uint16_t& outLength);
    std::string getStringFromPool(uint32_t offset, uint16_t length) const;
};

} // namespace sse::indexing
