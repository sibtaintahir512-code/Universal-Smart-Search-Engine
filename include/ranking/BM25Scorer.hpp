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
#include "ranking/DocumentLengthStatistics.hpp"

namespace sse::ranking {

/**
 * @brief Relevance scorer implementing Field-Weighted Best Matching 25 (BM25).
 */
class BM25Scorer {
public:
    /**
     * @brief Evaluates an individual BM25 term component:
     * IDF(t) * [ TF * (k1 + 1) ] / [ TF + k1 * (1 - b + b * (|d| / avgdl)) ]
     */
    static double computeTermScore(double tf,
                                  double idf,
                                  double docLength,
                                  double avgDocLength,
                                  double k1,
                                  double b) noexcept;

    /**
     * @brief Scores a document against positive query terms using Field-Weighted BM25.
     *
     * @param docId Target document identifier.
     * @param queryTerms Extracted positive query terms.
     * @param index Inverted index containing posting chains.
     * @param lengthStats Document length and corpus-wide average length provider.
     * @param totalDocs Total number of eligible indexed documents in the corpus (N).
     * @param config Ranking configuration (W_name, W_body, k1, b).
     * @param outExplanation Optional pointer to receive detailed explanation breakdown.
     * @return double Non-negative finite relevance score.
     */
    static double scoreDocument(DocID docId,
                                const dsa::CustomDynamicArray<ScoringTerm>& queryTerms,
                                const indexing::InvertedIndex& index,
                                const DocumentLengthStatistics& lengthStats,
                                std::size_t totalDocs,
                                const RankingConfig& config,
                                ScoreExplanation* outExplanation = nullptr);
};

} // namespace sse::ranking
