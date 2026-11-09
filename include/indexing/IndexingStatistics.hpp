#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace sse::indexing {

/**
 * @brief Telemetry metrics tracking performance and state of the indexing pipeline.
 */
struct IndexingStatistics {
    uint32_t documentsProcessed{0};
    uint32_t documentsFullyIndexed{0};
    uint32_t documentsPartiallyIndexed{0};
    uint32_t documentsFilenameOnly{0};
    uint32_t documentsFailed{0};

    uint64_t totalTokensProcessed{0};
    std::size_t uniqueVocabularyCount{0};
    uint64_t totalPostingsCount{0};

    uint64_t totalBodyTermOccurrences{0};
    uint64_t totalFilenameTermOccurrences{0};
    double   indexingDurationMs{0.0};

    void reset() noexcept {
        documentsProcessed = 0;
        documentsFullyIndexed = 0;
        documentsPartiallyIndexed = 0;
        documentsFilenameOnly = 0;
        documentsFailed = 0;
        totalTokensProcessed = 0;
        uniqueVocabularyCount = 0;
        totalPostingsCount = 0;
        totalBodyTermOccurrences = 0;
        totalFilenameTermOccurrences = 0;
        indexingDurationMs = 0.0;
    }

    double averageTokensPerDocument() const noexcept {
        if (documentsProcessed == 0) return 0.0;
        return static_cast<double>(totalTokensProcessed) / static_cast<double>(documentsProcessed);
    }

    std::string toString() const;
};

} // namespace sse::indexing
