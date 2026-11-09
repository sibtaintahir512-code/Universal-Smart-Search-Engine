#include "ranking/RankingEngine.hpp"
#include "ranking/TFIDFScorer.hpp"
#include "ranking/BM25Scorer.hpp"
#include "ranking/TopKSelector.hpp"
#include <algorithm>

namespace sse::ranking {

RankingEngine::RankingEngine(const indexing::InvertedIndex* index,
                             const indexing::DocumentCatalog* catalog,
                             const DocumentLengthStatistics* lengthStats,
                             RankingConfig config) noexcept
    : m_index(index), m_catalog(catalog), m_lengthStats(lengthStats), m_config(config) {}

void RankingEngine::collectPositiveTerms(const search::QueryNode* node,
                                         dsa::CustomDynamicArray<ScoringTerm>& outTerms,
                                         bool insideNot) {
    if (!node) return;

    switch (node->type()) {
        case search::QueryNodeType::Term: {
            if (!insideNot) {
                const auto* tNode = static_cast<const search::TermNode*>(node);
                ScoringTerm st(tNode->term, tNode->field);
                bool exists = false;
                for (std::size_t i = 0; i < outTerms.size(); ++i) {
                    if (outTerms[i] == st) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    outTerms.push_back(std::move(st));
                }
            }
            break;
        }

        case search::QueryNodeType::And: {
            const auto* andNode = static_cast<const search::AndNode*>(node);
            collectPositiveTerms(andNode->left.get(), outTerms, insideNot);
            collectPositiveTerms(andNode->right.get(), outTerms, insideNot);
            break;
        }

        case search::QueryNodeType::Or: {
            const auto* orNode = static_cast<const search::OrNode*>(node);
            collectPositiveTerms(orNode->left.get(), outTerms, insideNot);
            collectPositiveTerms(orNode->right.get(), outTerms, insideNot);
            break;
        }

        case search::QueryNodeType::Not: {
            const auto* notNode = static_cast<const search::NotNode*>(node);
            // Invert the negation context: terms inside NOT are excluded from positive scoring
            collectPositiveTerms(notNode->operand.get(), outTerms, true);
            break;
        }
    }
}

dsa::CustomDynamicArray<ScoringTerm> RankingEngine::extractPositiveQueryTerms(const search::QueryNode* root) {
    dsa::CustomDynamicArray<ScoringTerm> terms;
    collectPositiveTerms(root, terms, false);
    return terms;
}

dsa::CustomDynamicArray<ScoredDocument> RankingEngine::rankCandidates(
    const search::ResultSet& candidates,
    const search::QueryNode* queryAst,
    std::size_t topK) {

    dsa::CustomDynamicArray<ScoredDocument> result;
    if (candidates.empty()) {
        return result;
    }

    std::size_t effectiveK = (topK == 0 || topK > candidates.size()) ? candidates.size() : topK;

    // 1. DocIdOrder Mode: Preserve Phase 6 deterministic ascending DocID baseline
    if (m_config.mode == RankingMode::DocIdOrder) {
        result.reserve(effectiveK);
        for (std::size_t i = 0; i < effectiveK; ++i) {
            result.push_back(ScoredDocument(candidates[i], 0.0, static_cast<uint32_t>(i + 1)));
        }
        m_statistics.recordRanking(RankingMode::DocIdOrder, effectiveK, 0.0, 0.0, false);
        return result;
    }

    // 2. Validate configuration
    if (!m_config.isValid() || !m_index || !m_catalog) {
        m_statistics.recordInvalidConfiguration();
        // Fallback to DocIdOrder on invalid configuration
        result.reserve(effectiveK);
        for (std::size_t i = 0; i < effectiveK; ++i) {
            result.push_back(ScoredDocument(candidates[i], 0.0, static_cast<uint32_t>(i + 1)));
        }
        m_statistics.recordRanking(m_config.mode, effectiveK, 0.0, 0.0, true);
        return result;
    }

    // 3. Extract positive scoring terms from query AST
    dsa::CustomDynamicArray<ScoringTerm> positiveTerms = extractPositiveQueryTerms(queryAst);

    // 4. Pure negation check: if no positive terms exist (e.g. "NOT apple"), use neutral DocID ordering
    if (positiveTerms.empty()) {
        result.reserve(effectiveK);
        for (std::size_t i = 0; i < effectiveK; ++i) {
            result.push_back(ScoredDocument(candidates[i], 0.0, static_cast<uint32_t>(i + 1)));
        }
        m_statistics.recordRanking(m_config.mode, effectiveK, 0.0, 0.0, false);
        return result;
    }

    // 5. Score all matching candidates
    auto tScoringStart = std::chrono::high_resolution_clock::now();

    dsa::CustomDynamicArray<ScoredDocument> scoredCandidates;
    scoredCandidates.reserve(candidates.size());

    std::size_t totalDocs = m_catalog->activeCount();
    if (totalDocs == 0) {
        totalDocs = m_catalog->totalCount();
    }

    for (std::size_t i = 0; i < candidates.size(); ++i) {
        DocID id = candidates[i];
        std::shared_ptr<ScoreExplanation> expl = nullptr;
        if (m_config.enableExplanation) {
            expl = std::make_shared<ScoreExplanation>();
        }

        double score = 0.0;
        if (m_config.mode == RankingMode::TFIDF) {
            score = TFIDFScorer::scoreDocument(id, positiveTerms, *m_index, totalDocs, m_config, expl.get());
        } else if (m_config.mode == RankingMode::BM25) {
            if (m_lengthStats) {
                score = BM25Scorer::scoreDocument(id, positiveTerms, *m_index, *m_lengthStats,
                                                  totalDocs, m_config, expl.get());
            } else {
                // Fallback to TF-IDF if length statistics are unavailable
                score = TFIDFScorer::scoreDocument(id, positiveTerms, *m_index, totalDocs, m_config, expl.get());
            }
        }

        scoredCandidates.push_back(ScoredDocument(id, score, 0, std::move(expl)));
    }

    auto tScoringEnd = std::chrono::high_resolution_clock::now();
    double scoringMs = std::chrono::duration<double, std::milli>(tScoringEnd - tScoringStart).count();

    // 6. Select Top-K via Bounded CustomMinHeap
    auto tTopKStart = std::chrono::high_resolution_clock::now();
    result = TopKSelector::selectTopK(scoredCandidates, effectiveK);
    auto tTopKEnd = std::chrono::high_resolution_clock::now();
    double topKMs = std::chrono::duration<double, std::milli>(tTopKEnd - tTopKStart).count();

    // 7. Record Telemetry
    m_statistics.recordRanking(m_config.mode, result.size(), scoringMs, topKMs, false);

    return result;
}

} // namespace sse::ranking
