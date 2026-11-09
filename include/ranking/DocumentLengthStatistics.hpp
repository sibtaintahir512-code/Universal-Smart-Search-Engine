#pragma once

#include <cstdint>
#include <cstddef>
#include <shared_mutex>
#include <mutex>
#include "core/CommonTypes.hpp"
#include "dsa/CustomDynamicArray.hpp"

namespace sse::ranking {

/**
 * @brief Per-document token length record.
 */
struct DocLengthEntry {
    uint32_t nameTokens{0};
    uint32_t bodyTokens{0};
    bool     isActive{false};
};

/**
 * @brief High-performance document length tracking and corpus-wide length statistics.
 *
 * Maintains accurate token counts for filename and extracted body per DocID in O(1) time.
 * Calculates exact average document lengths (avgdl) required by BM25.
 * Coordinates seamlessly with document ingestion, updates, and removal.
 */
class DocumentLengthStatistics {
public:
    DocumentLengthStatistics() = default;
    ~DocumentLengthStatistics() = default;

    // Rule of Five (Non-copyable due to mutex synchronization, movable)
    DocumentLengthStatistics(const DocumentLengthStatistics&) = delete;
    DocumentLengthStatistics& operator=(const DocumentLengthStatistics&) = delete;
    DocumentLengthStatistics(DocumentLengthStatistics&& other) noexcept;
    DocumentLengthStatistics& operator=(DocumentLengthStatistics&& other) noexcept;

    /**
     * @brief Records or updates token counts for a given document.
     */
    void recordDocumentLength(DocID docId, uint32_t nameTokens, uint32_t bodyTokens);

    /**
     * @brief Incrementally accumulates term frequencies into a document's token lengths.
     */
    void accumulateDocumentLength(DocID docId, uint32_t nameFreq, uint32_t bodyFreq);

    /**
     * @brief Removes a document's token counts and updates running corpus statistics.
     */
    bool removeDocument(DocID docId);

    /**
     * @brief Clears all document length records and resets running statistics.
     */
    void clear() noexcept;

    // =========================================================================
    // Retrieval APIs
    // =========================================================================

    uint32_t getNameLength(DocID docId) const noexcept;
    uint32_t getBodyLength(DocID docId) const noexcept;
    uint32_t getTotalLength(DocID docId) const noexcept;
    bool     hasDocument(DocID docId) const noexcept;

    std::size_t activeDocumentCount() const noexcept;
    uint64_t totalNameTokens() const noexcept;
    uint64_t totalBodyTokens() const noexcept;

    double averageNameLength() const noexcept;
    double averageBodyLength() const noexcept;
    double averageTotalLength() const noexcept;

private:
    mutable std::shared_mutex               m_mutex;
    dsa::CustomDynamicArray<DocLengthEntry> m_lengths; // 1-based: index = docId - 1
    uint64_t                                m_totalNameTokens{0};
    uint64_t                                m_totalBodyTokens{0};
    std::size_t                             m_activeDocs{0};
};

} // namespace sse::ranking
