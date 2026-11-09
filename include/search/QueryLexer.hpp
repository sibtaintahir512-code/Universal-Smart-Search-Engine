#pragma once

#include <string>
#include <string_view>
#include <cstddef>
#include "dsa/CustomDynamicArray.hpp"
#include "search/QueryToken.hpp"
#include "search/SearchConfig.hpp"

namespace sse::search {

/**
 * @brief Result object returned by QueryLexer::tokenize.
 */
struct LexResult {
    bool                             success{false};
    std::string                      errorMessage;
    std::size_t                      errorOffset{0};
    dsa::CustomDynamicArray<QueryToken> tokens;
};

/**
 * @brief Deterministic lexical analyzer for search queries.
 *
 * Scans raw query strings into discrete QueryToken sequences.
 * Enforces UTF-8 compliance, detects Boolean operators, field qualifiers,
 * grouping parentheses, and explicitly rejects unsupported syntax (wildcards, fuzzy).
 */
class QueryLexer {
public:
    explicit QueryLexer(SearchConfig config = SearchConfig{}) noexcept
        : m_config(config) {}

    /**
     * @brief Tokenizes raw input query string.
     */
    LexResult tokenize(std::string_view query) const;

private:
    SearchConfig m_config;

    static bool isWhitespace(char c) noexcept;
    static bool isDelimiter(char c) noexcept;
    static std::string toUpperAscii(std::string_view s);
};

} // namespace sse::search
