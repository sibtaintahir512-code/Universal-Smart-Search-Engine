#pragma once

#include <cstdint>
#include <string>
#include <cmath>
#include <algorithm>
#include "core/CommonTypes.hpp"
#include "indexing/InvertedIndex.hpp"
#include "ranking/RankingConfig.hpp"
#include "ranking/RankingResult.hpp"
#include "ranking/ScoringTerm.hpp"

namespace sse::ranking {

/**
 * @brief Relevance scorer implementing Field-Weighted TF-IDF.
 */
class TFIDFScorer {
public:
    /**
     * @brief Computes BM25-style smoothed Inverse Document Frequency:
     * IDF(t) = ln(1 + (N - DF(t) + 0.5) / (DF(t) + 0.5))
     */
    static double computeBM25SmoothedIdf(std::size_t totalDocs, uint32_t docFreq) noexcept;

    /**
     * @brief Computes conventional smoothed Inverse Document Frequency:
     * IDF(t) = ln((N + 1) / (DF(t) + 1)) + 1
     */
    static double computeConventionalIdf(std::size_t totalDocs, uint32_t docFreq) noexcept;

    /**
     * @brief Scores a document against positive query terms using Field-Weighted TF-IDF.
     *
     * @param docId Target document identifier.
     * @param queryTerms Extracted positive query terms.
     * @param index Inverted index containing posting chains.
     * @param totalDocs Total number of eligible indexed documents in the corpus (N).
     * @param config Ranking configuration specifying field weights (W_name, W_body).
     * @param outExplanation Optional pointer to receive detailed explanation breakdown.
     * @return double Non-negative finite relevance score.
     */
    static double scoreDocument(DocID docId,
                                const dsa::CustomDynamicArray<ScoringTerm>& queryTerms,
                                const indexing::InvertedIndex& index,
                                std::size_t totalDocs,
                                const RankingConfig& config,
                                ScoreExplanation* outExplanation = nullptr);
};

} // namespace sse::ranking
