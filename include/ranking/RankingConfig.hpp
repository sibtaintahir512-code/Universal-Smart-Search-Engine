#pragma once

#include <cstdint>
#include <cmath>

namespace sse::ranking {

/**
 * @brief Ranking algorithm selection.
 */
enum class RankingMode : uint8_t {
    DocIdOrder = 0, // Unranked Phase 6 baseline (ascending DocID)
    TFIDF,          // Field-weighted Term Frequency - Inverse Document Frequency
    BM25            // Field-weighted Best Matching 25 probabilistic relevance
};

/**
 * @brief Configuration parameters for the relevance ranking engine.
 */
struct RankingConfig {
    RankingMode mode{RankingMode::DocIdOrder};
    double      filenameWeight{5.0};    // W_name: Default 5.0 per Phase 0 design
    double      bodyWeight{1.0};        // W_body: Default 1.0 per Phase 0 design
    double      bm25K1{1.2};            // k1: Term-frequency saturation parameter (k1 > 0)
    double      bm25B{0.75};            // b: Document length normalization parameter (0 <= b <= 1)
    bool        enableExplanation{false}; // Generate detailed scoring explanations

    /**
     * @brief Validates parameter ranges and numerical sanity.
     */
    bool isValid() const noexcept {
        if (!std::isfinite(filenameWeight) || filenameWeight < 0.0) return false;
        if (!std::isfinite(bodyWeight) || bodyWeight < 0.0) return false;
        if (!std::isfinite(bm25K1) || bm25K1 <= 0.0) return false;
        if (!std::isfinite(bm25B) || bm25B < 0.0 || bm25B > 1.0) return false;
        return true;
    }
};

} // namespace sse::ranking
