#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include "persistence/PersistenceResult.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"

namespace sse::persistence {

/**
 * @brief Zero-dangling-reference index compaction & reference remapping subsystem.
 *
 * Implements the approved Phase 0 two-pass reference remapping algorithm:
 * - Purges tombstoned (soft-deleted) documents.
 * - Re-indexes active documents into a contiguous DocID range [1 .. N_active].
 * - Rewrites all posting lists and document length records to point to new remapped IDs.
 * - Eliminates empty vocabulary entries whose postings were entirely tombstoned.
 * - Guarantees transactionality: live state is unmodified until full success.
 */
class IndexCompactor {
public:
    IndexCompactor() = default;

    /**
     * @brief Performs compaction from srcCatalog and srcIndex into outCatalog and outIndex.
     *
     * @param srcCatalog Source catalog (may contain tombstones).
     * @param srcIndex Source inverted index.
     * @param outCatalog Output catalog receiving compacted records.
     * @param outIndex Output inverted index receiving remapped postings and lengths.
     * @param cancelToken Optional cancellation flag.
     * @return PersistenceResult Operation summary.
     */
    static PersistenceResult compact(
        const indexing::DocumentCatalog& srcCatalog,
        const indexing::InvertedIndex& srcIndex,
        indexing::DocumentCatalog& outCatalog,
        indexing::InvertedIndex& outIndex,
        const std::atomic<bool>* cancelToken = nullptr);
};

} // namespace sse::persistence
