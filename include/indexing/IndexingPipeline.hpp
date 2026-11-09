#pragma once

#include <string>
#include <memory>
#include <chrono>
#include "core/CommonTypes.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include "indexing/Tokenizer.hpp"
#include "indexing/IndexingStatistics.hpp"
#include "extraction/ExtractionManager.hpp"
#include "extraction/ExtractionResult.hpp"

namespace sse::indexing {

/**
 * @brief Result summary returned after indexing an individual document.
 */
struct IndexingResult {
    DocID       docId{INVALID_DOC_ID};
    std::string filePath;
    bool        success{false};
    bool        isPartial{false};
    bool        filenameOnly{false};
    uint32_t    tokensIndexed{0};
    uint32_t    uniqueTermsIndexed{0};
    double      durationMs{0.0};
    std::string message;

    IndexingResult() = default;
};

/**
 * @brief Orchestration pipeline connecting DocumentCatalog, Content Extraction,
 * Tokenization, and InvertedIndex.
 */
class IndexingPipeline {
public:
    IndexingPipeline(DocumentCatalog& catalog,
                     InvertedIndex& index,
                     Tokenizer tokenizer = Tokenizer{},
                     extraction::ExtractionManager* extractionManager = nullptr);
    ~IndexingPipeline() = default;

    // Non-copyable, non-movable (binds references to catalog and index)
    IndexingPipeline(const IndexingPipeline&) = delete;
    IndexingPipeline& operator=(const IndexingPipeline&) = delete;
    IndexingPipeline(IndexingPipeline&&) = delete;
    IndexingPipeline& operator=(IndexingPipeline&&) = delete;

    /**
     * @brief Indexes a document given its DocID.
     * @param docId Registered active document identifier in DocumentCatalog.
     * @param optExtraction Optional pre-extracted content; if null, queries ExtractionManager.
     * @return Detailed IndexingResult describing operation outcome.
     */
    IndexingResult indexDocument(DocID docId, const extraction::ExtractionResult* optExtraction = nullptr);

    /**
     * @brief Updates an existing document in the index without creating duplicate postings.
     * Atomically clears previous postings for this DocID and re-indexes new content.
     */
    IndexingResult updateDocument(DocID docId, const extraction::ExtractionResult* optExtraction = nullptr);

    /**
     * @brief Removes a document's postings from the inverted index and marks it deleted in the catalog.
     */
    bool removeDocument(DocID docId);

    // =========================================================================
    // Accessors
    // =========================================================================

    const IndexingStatistics& statistics() const noexcept { return m_stats; }
    void resetStatistics() noexcept { m_stats.reset(); }

    const InvertedIndex& index() const noexcept { return m_index; }
    InvertedIndex& mutableIndex() noexcept { return m_index; }

    const DocumentCatalog& catalog() const noexcept { return m_catalog; }
    const Tokenizer& tokenizer() const noexcept { return m_tokenizer; }

private:
    DocumentCatalog&               m_catalog;
    InvertedIndex&                 m_index;
    Tokenizer                      m_tokenizer;
    extraction::ExtractionManager* m_extractionManager{nullptr};
    IndexingStatistics             m_stats;
};

} // namespace sse::indexing
