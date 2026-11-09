#pragma once

#include <memory>
#include <string>
#include <cstddef>
#include "dsa/CustomDynamicArray.hpp"
#include "search/QueryToken.hpp"
#include "search/QueryNode.hpp"
#include "search/SearchConfig.hpp"

namespace sse::search {

/**
 * @brief Parse result containing AST root and diagnostic status.
 */
struct ParseResult {
    bool                       success{false};
    std::string                errorMessage;
    std::size_t                errorOffset{0};
    std::unique_ptr<QueryNode> root;
};

/**
 * @brief Recursive-descent parser producing an Abstract Syntax Tree (AST)
 * from a lexical QueryToken stream.
 *
 * Implements strict Boolean operator precedence:
 *   1. Parentheses (innermost evaluated first)
 *   2. Unary NOT (highest precedence, right-to-left)
 *   3. Binary AND and implicit AND (left-to-right)
 *   4. Binary OR (lowest precedence, left-to-right)
 *
 * Enforces AST safety limits (depth, node count) and guarantees memory safety
 * via RAII smart pointers.
 */
class QueryParser {
public:
    explicit QueryParser(SearchConfig config = SearchConfig{}) noexcept
        : m_config(config) {}

    /**
     * @brief Parses a stream of query tokens into an AST.
     */
    ParseResult parse(const dsa::CustomDynamicArray<QueryToken>& tokens);

private:
    SearchConfig                              m_config;
    const dsa::CustomDynamicArray<QueryToken>* m_tokens{nullptr};
    std::size_t                               m_current{0};
    std::string                               m_errorMessage;
    std::size_t                               m_errorOffset{0};

    std::unique_ptr<QueryNode> parseExpression();
    std::unique_ptr<QueryNode> parseOrExpression();
    std::unique_ptr<QueryNode> parseAndExpression();
    std::unique_ptr<QueryNode> parseUnaryExpression();
    std::unique_ptr<QueryNode> parsePrimary();

    bool isAtEnd() const noexcept;
    const QueryToken& peek() const noexcept;
    const QueryToken& previous() const noexcept;
    bool check(QueryTokenType type) const noexcept;
    bool match(QueryTokenType type) noexcept;
    const QueryToken& advance() noexcept;

    void setError(std::string msg, std::size_t offset);
    static bool canStartUnary(QueryTokenType type) noexcept;
};

} // namespace sse::search
