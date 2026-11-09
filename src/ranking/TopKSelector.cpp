#include "ranking/TopKSelector.hpp"

namespace sse::ranking {

dsa::CustomDynamicArray<ScoredDocument> TopKSelector::selectTopK(
    const dsa::CustomDynamicArray<ScoredDocument>& candidates,
    std::size_t k) {

    dsa::CustomDynamicArray<ScoredDocument> result;
    if (candidates.empty()) {
        return result;
    }

    ScoredDocumentMinHeapComparator isBetter;
    ScoredDocumentDescendingComparator descComp;

    // Fast path: k == 0 (rank all) or requested K >= total candidates
    if (k == 0 || candidates.size() <= k) {
        result = candidates;
        std::sort(result.begin(), result.end(), descComp);
        for (std::size_t i = 0; i < result.size(); ++i) {
            result[i].rank = static_cast<uint32_t>(i + 1);
        }
        return result;
    }

    // Bounded Min-Heap of size K
    dsa::CustomMinHeap<ScoredDocument, ScoredDocumentMinHeapComparator> minHeap;
    minHeap.reserve(k);

    for (std::size_t i = 0; i < k; ++i) {
        minHeap.push(candidates[i]);
    }

    for (std::size_t i = k; i < candidates.size(); ++i) {
        const ScoredDocument& cand = candidates[i];
        // If candidate is better than the weakest element at root, replace it
        if (isBetter(cand, minHeap.top())) {
            minHeap.replaceTop(cand);
        }
    }

    // Extract all K elements (extractTop yields weakest to strongest)
    result.reserve(k);
    for (std::size_t i = 0; i < k; ++i) {
        result.push_back(minHeap.extractTop());
    }

    // Reverse to achieve strictly descending order (strongest to weakest)
    std::reverse(result.begin(), result.end());

    // Assign 1-based ranks
    for (std::size_t i = 0; i < result.size(); ++i) {
        result[i].rank = static_cast<uint32_t>(i + 1);
    }

    return result;
}

} // namespace sse::ranking
