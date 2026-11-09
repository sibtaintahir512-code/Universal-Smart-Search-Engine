#pragma once

#include <string>
#include <cstddef>
#include <cstdint>

namespace sse::search {

/**
 * @brief Categorization of lexical query tokens.
 */
enum class QueryTokenType : uint8_t {
    Term = 0,      // Word or term token (e.g. "search", "engine")
    And,           // Logical AND conjunction ("AND", "and")
    Or,            // Logical OR disjunction ("OR", "or")
    Not,           // Logical NOT negation ("NOT", "not")
    LeftParen,     // Grouping open parenthesis '('
    RightParen,    // Grouping close parenthesis ')'
    EndOfInput,    // End of query string sentinel
    Invalid        // Malformed, unsupported, or invalid token
};

/**
 * @brief Search field qualifier for field-restricted queries.
 */
enum class FieldQualifier : uint8_t {
    Unspecified = 0, // Search both filename and content (default)
    Filename,        // Search only filename ("name:term")
    Body             // Search only extracted document body ("body:term")
};

/**
 * @brief Lexical token produced by the query lexer.
 */
struct QueryToken {
    QueryTokenType  type{QueryTokenType::Invalid};
    std::string     value;                          // Normalized term value or operator literal
    std::string     raw;                            // Raw source slice for error reporting
    FieldQualifier  field{FieldQualifier::Unspecified}; // Field qualifier
    std::size_t     position{0};                    // 0-based token index in stream
    std::size_t     charOffset{0};                  // 0-based byte offset in query string
    std::size_t     length{0};                      // Length in characters

    QueryToken() = default;

    QueryToken(QueryTokenType t,
               std::string val,
               std::string rawSlice,
               FieldQualifier fld,
               std::size_t pos,
               std::size_t offset,
               std::size_t len)
        : type(t)
        , value(std::move(val))
        , raw(std::move(rawSlice))
        , field(fld)
        , position(pos)
        , charOffset(offset)
        , length(len) {}
};

} // namespace sse::search
