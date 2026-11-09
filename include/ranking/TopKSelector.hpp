#pragma once

#include <cstddef>
#include <algorithm>
#include "dsa/CustomDynamicArray.hpp"
#include "dsa/CustomMinHeap.hpp"
#include "ranking/RankingResult.hpp"

namespace sse::ranking {

/**
 * @brief Algorithmic Top-K selector utilizing Bounded CustomMinHeap.
 *
 * Extracts the highest-scoring K documents in O(M log K) time without
 * sorting the entire candidate set. Enforces deterministic tie-breaking.
 */
class TopKSelector {
public:
    /**
     * @brief Selects the Top-K scored documents in strictly descending relevance order.
     *
     * @param candidates Unsorted candidate list of scored documents.
     * @param k Target number of top documents to retain.
     * @return dsa::CustomDynamicArray<ScoredDocument> Retained documents in descending order.
     */
    static dsa::CustomDynamicArray<ScoredDocument> selectTopK(
        const dsa::CustomDynamicArray<ScoredDocument>& candidates,
        std::size_t k);
};

} // namespace sse::ranking
