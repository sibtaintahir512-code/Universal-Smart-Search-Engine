#pragma once

#include <atomic>
#include <string>
#include <cstdint>
#include "indexing/InvertedIndex.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "search/QueryNode.hpp"
#include "search/ResultSet.hpp"

namespace sse::search {

/**
 * @brief Diagnostic telemetry tracked during AST evaluation.
 */
struct EvaluationDiagnostics {
    uint64_t termsEvaluated{0};
    uint64_t booleanOperationsEvaluated{0};
};

/**
 * @brief Output of QueryEvaluator execution.
 */
struct EvaluationResult {
    bool                  success{false};
    bool                  cancelled{false};
    std::string           errorMessage;
    ResultSet             resultSet;
    EvaluationDiagnostics diagnostics;
};

/**
 * @brief Evaluates parsed Query AST against the Inverted Index and Document Catalog.
 *
 * Implements:
 * - O(1) posting list retrieval per search term.
 * - Field-restricted occurrence filtering (name:, body:, or unqualified).
 * - Linear two-pointer Boolean intersection (AND) and union (OR).
 * - Document-universe difference for NOT expressions (U \ S).
 * - Tombstone and catalog validation ensuring all returned DocIDs are active.
 * - Cooperative cancellation checks.
 */
class QueryEvaluator {
public:
    QueryEvaluator(const indexing::InvertedIndex* index,
                   const indexing::DocumentCatalog* catalog) noexcept
        : m_index(index), m_catalog(catalog) {}

    /**
     * @brief Evaluates an AST root node.
     * @param root Root node of parsed query AST.
     * @param cancelToken Optional atomic cancellation flag.
     */
    EvaluationResult evaluate(const QueryNode* root,
                             const std::atomic<bool>* cancelToken = nullptr) const;

private:
    const indexing::InvertedIndex*   m_index{nullptr};
    const indexing::DocumentCatalog* m_catalog{nullptr};

    ResultSet evaluateNode(const QueryNode* node,
                           const std::atomic<bool>* cancelToken,
                           EvaluationDiagnostics& diag,
                           bool& cancelled) const;

    ResultSet evaluateTerm(const TermNode* termNode,
                           EvaluationDiagnostics& diag) const;

    ResultSet buildUniverse() const;
};

} // namespace sse::search
