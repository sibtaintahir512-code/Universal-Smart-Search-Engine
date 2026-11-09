#include "search/QueryEvaluator.hpp"

namespace sse::search {

EvaluationResult QueryEvaluator::evaluate(const QueryNode* root,
                                         const std::atomic<bool>* cancelToken) const {
    EvaluationResult res;

    if (!root) {
        res.success = false;
        res.errorMessage = "Null AST root provided for query evaluation";
        return res;
    }

    if (!m_index || !m_catalog) {
        res.success = false;
        res.errorMessage = "QueryEvaluator: InvertedIndex or DocumentCatalog dependency is null";
        return res;
    }

    if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
        res.success = false;
        res.cancelled = true;
        res.errorMessage = "Query evaluation cancelled prior to execution";
        return res;
    }

    bool cancelled = false;
    res.resultSet = evaluateNode(root, cancelToken, res.diagnostics, cancelled);

    if (cancelled) {
        res.success = false;
        res.cancelled = true;
        res.errorMessage = "Query evaluation cancelled by user";
        res.resultSet.clear();
        return res;
    }

    res.success = true;
    return res;
}

ResultSet QueryEvaluator::evaluateNode(const QueryNode* node,
                                      const std::atomic<bool>* cancelToken,
                                      EvaluationDiagnostics& diag,
                                      bool& cancelled) const {
    if (!node || cancelled) {
        return ResultSet{};
    }

    if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
        cancelled = true;
        return ResultSet{};
    }

    switch (node->type()) {
        case QueryNodeType::Term: {
            return evaluateTerm(static_cast<const TermNode*>(node), diag);
        }

        case QueryNodeType::And: {
            const auto* andNode = static_cast<const AndNode*>(node);
            ResultSet left = evaluateNode(andNode->left.get(), cancelToken, diag, cancelled);
            if (cancelled) return ResultSet{};

            // Short-circuit: empty intersection is always empty
            if (left.empty()) {
                ++diag.booleanOperationsEvaluated;
                return ResultSet{};
            }

            ResultSet right = evaluateNode(andNode->right.get(), cancelToken, diag, cancelled);
            if (cancelled) return ResultSet{};

            ++diag.booleanOperationsEvaluated;
            return ResultSet::intersect(left, right);
        }

        case QueryNodeType::Or: {
            const auto* orNode = static_cast<const OrNode*>(node);
            ResultSet left = evaluateNode(orNode->left.get(), cancelToken, diag, cancelled);
            if (cancelled) return ResultSet{};

            ResultSet right = evaluateNode(orNode->right.get(), cancelToken, diag, cancelled);
            if (cancelled) return ResultSet{};

            ++diag.booleanOperationsEvaluated;
            return ResultSet::unionSets(left, right);
        }

        case QueryNodeType::Not: {
            const auto* notNode = static_cast<const NotNode*>(node);
            ResultSet operand = evaluateNode(notNode->operand.get(), cancelToken, diag, cancelled);
            if (cancelled) return ResultSet{};

            ResultSet universe = buildUniverse();
            ++diag.booleanOperationsEvaluated;
            return ResultSet::difference(universe, operand);
        }
    }

    return ResultSet{};
}

ResultSet QueryEvaluator::evaluateTerm(const TermNode* termNode,
                                      EvaluationDiagnostics& diag) const {
    ResultSet result;
    if (!m_index || !m_catalog || !termNode) {
        return result;
    }

    ++diag.termsEvaluated;

    const dsa::CustomPostingList* postList = m_index->getPostingList(termNode->term);
    if (!postList || postList->empty()) {
        return result;
    }

    const PostingNode* curr = postList->head();
    while (curr) {
        DocID docId = curr->docId;

        // Invariant: Verify DocID is currently active in DocumentCatalog
        if (m_catalog->isActive(docId)) {
            bool matches = false;

            if (termNode->field == FieldQualifier::Filename) {
                matches = ((curr->fieldMask & FIELD_FILENAME) != 0) || (curr->nameTermFreq > 0);
            } else if (termNode->field == FieldQualifier::Body) {
                matches = ((curr->fieldMask & FIELD_CONTENT) != 0) || (curr->bodyTermFreq > 0);
            } else {
                // Unspecified: match if present in either field
                matches = true;
            }

            if (matches) {
                result.add(docId);
            }
        }

        curr = curr->next;
    }

    return result;
}

ResultSet QueryEvaluator::buildUniverse() const {
    ResultSet universe;
    if (!m_catalog) return universe;

    const std::size_t total = m_catalog->totalCount();
    for (DocID id = 1; id <= total; ++id) {
        if (m_catalog->isActive(id)) {
            universe.add(id);
        }
    }

    return universe;
}

} // namespace sse::search
