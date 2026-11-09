#pragma once

#include <cstddef>
#include <chrono>
#include "core/CommonTypes.hpp"
#include "dsa/CustomDynamicArray.hpp"
#include "indexing/InvertedIndex.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "search/ResultSet.hpp"
#include "search/QueryNode.hpp"
#include "ranking/RankingConfig.hpp"
#include "ranking/RankingResult.hpp"
#include "ranking/ScoringTerm.hpp"
#include "ranking/DocumentLengthStatistics.hpp"
#include "ranking/RankingStatistics.hpp"

namespace sse::ranking {

/**
 * @brief Central relevance ranking engine orchestrating positive query-term
 * extraction, field-aware scoring, and bounded Top-K priority heap selection.
 */
class RankingEngine {
public:
    RankingEngine(const indexing::InvertedIndex* index,
                  const indexing::DocumentCatalog* catalog,
                  const DocumentLengthStatistics* lengthStats,
                  RankingConfig config = RankingConfig{}) noexcept;

    ~RankingEngine() = default;

    // Rule of Five (Non-copyable due to raw pointers, movable)
    RankingEngine(const RankingEngine&) = delete;
    RankingEngine& operator=(const RankingEngine&) = delete;
    RankingEngine(RankingEngine&&) noexcept = default;
    RankingEngine& operator=(RankingEngine&&) noexcept = default;

    /**
     * @brief Extracts normalized positive query terms from AST, excluding terms inside NOT branches.
     */
    static dsa::CustomDynamicArray<ScoringTerm> extractPositiveQueryTerms(const search::QueryNode* root);

    /**
     * @brief Ranks matching candidate documents by relevance score and extracts Top-K results.
     *
     * @param candidates Matching DocIDs satisfying Boolean query constraints.
     * @param queryAst Parsed query AST used to extract positive scoring terms.
     * @param topK Number of top results to extract (0 = rank all candidates).
     * @return dsa::CustomDynamicArray<ScoredDocument> Ranked documents in descending order.
     */
    dsa::CustomDynamicArray<ScoredDocument> rankCandidates(
        const search::ResultSet& candidates,
        const search::QueryNode* queryAst,
        std::size_t topK = 0);

    /**
     * @brief Configures ranking parameters and algorithm mode.
     */
    void setConfig(RankingConfig config) noexcept { m_config = config; }
    const RankingConfig& config() const noexcept { return m_config; }

    /**
     * @brief Access cumulative ranking statistics.
     */
    const RankingStatistics& statistics() const noexcept { return m_statistics; }
    RankingStatistics& statistics() noexcept { return m_statistics; }
    void resetStatistics() noexcept { m_statistics.reset(); }

private:
    const indexing::InvertedIndex*   m_index{nullptr};
    const indexing::DocumentCatalog* m_catalog{nullptr};
    const DocumentLengthStatistics*  m_lengthStats{nullptr};
    RankingConfig                    m_config;
    RankingStatistics                m_statistics;

    static void collectPositiveTerms(const search::QueryNode* node,
                                     dsa::CustomDynamicArray<ScoringTerm>& outTerms,
                                     bool insideNot);
};

} // namespace sse::ranking
