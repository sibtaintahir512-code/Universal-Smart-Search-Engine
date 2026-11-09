#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include <cmath>
#include "core/CommonTypes.hpp"
#include "dsa/CustomDynamicArray.hpp"
#include "search/QueryToken.hpp"
#include "ranking/RankingConfig.hpp"

namespace sse::ranking {

/**
 * @brief Detailed breakdown of an individual term's scoring contribution.
 */
struct TermExplanation {
    std::string           term;
    search::FieldQualifier field{search::FieldQualifier::Unspecified};
    uint32_t              df{0};
    double                idf{0.0};
    uint16_t              nameTf{0};
    uint16_t              bodyTf{0};
    double                nameScore{0.0};
    double                bodyScore{0.0};
    double                totalTermScore{0.0};
};

/**
 * @brief Full scoring explanation for a document.
 */
struct ScoreExplanation {
    DocID                                  docId{INVALID_DOC_ID};
    RankingMode                            mode{RankingMode::BM25};
    double                                 finalScore{0.0};
    double                                 filenameContribution{0.0};
    double                                 bodyContribution{0.0};
    double                                 filenameWeight{5.0};
    double                                 bodyWeight{1.0};
    uint32_t                               documentBodyLength{0};
    uint32_t                               documentNameLength{0};
    double                                 averageBodyLength{0.0};
    double                                 averageNameLength{0.0};
    dsa::CustomDynamicArray<TermExplanation> termDetails;
};

/**
 * @brief Represents a scored document in the ranking pipeline.
 */
struct ScoredDocument {
    DocID                              docId{INVALID_DOC_ID};
    double                             score{0.0};
    uint32_t                           rank{0};
    std::shared_ptr<ScoreExplanation>  explanation{nullptr};

    ScoredDocument() = default;

    ScoredDocument(DocID id, double s, uint32_t r = 0, std::shared_ptr<ScoreExplanation> expl = nullptr)
        : docId(id), score(s), rank(r), explanation(std::move(expl)) {}
};

/**
 * @brief Min-Heap Comparator for Bounded Top-K selection.
 *
 * In CustomMinHeap, m_comp(a, b) == true means 'a' is "greater than" 'b',
 * so 'b' will be placed at the root of the heap (the minimum / weakest candidate).
 *
 * For relevance ranking:
 * Candidate A is BETTER than Candidate B (A > B) if:
 * 1. A.score > B.score (with 1e-9 epsilon tolerance)
 * 2. On tie (scores equal): A.docId < B.docId (lower DocID preferred)
 *
 * Therefore, if A is better than B, operator()(A, B) returns true.
 * This guarantees the WEAKEST candidate in the Top-K set sits at the heap root!
 */
struct ScoredDocumentMinHeapComparator {
    bool operator()(const ScoredDocument& a, const ScoredDocument& b) const noexcept {
        constexpr double EPSILON = 1e-9;
        if (std::fabs(a.score - b.score) > EPSILON) {
            return a.score > b.score;
        }
        // Tie-breaker: lower DocID is better, so 'a' is greater/better than 'b' if a.docId < b.docId
        return a.docId < b.docId;
    }
};

/**
 * @brief Final Descending Comparator for output ordering.
 * Best candidate (highest score, lowest DocID on tie) comes first.
 */
struct ScoredDocumentDescendingComparator {
    bool operator()(const ScoredDocument& a, const ScoredDocument& b) const noexcept {
        constexpr double EPSILON = 1e-9;
        if (std::fabs(a.score - b.score) > EPSILON) {
            return a.score > b.score;
        }
        return a.docId < b.docId;
    }
};

} // namespace sse::ranking
