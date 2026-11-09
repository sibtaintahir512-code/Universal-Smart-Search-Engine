#pragma once

#include <cstddef>

namespace sse::search {

/**
 * @brief Safety limits and execution parameters for the search subsystem.
 */
struct SearchConfig {
    std::size_t maxQueryLength{1024}; // Maximum character length of raw query string
    std::size_t maxQueryTokens{256};  // Maximum number of lexical tokens permitted
    std::size_t maxAstDepth{32};      // Maximum allowable AST expression nesting depth
    std::size_t maxAstNodes{256};     // Maximum allowable AST node count
    std::size_t defaultLimit{50};     // Default result page size (0 = unlimited)
};

} // namespace sse::search
