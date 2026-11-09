#pragma once

#include <string>
#include <cstddef>
#include <cstdint>
#include "core/CommonTypes.hpp"
#include "dsa/CustomDynamicArray.hpp"
#include "search/SearchResult.hpp"

namespace sse::search {

/**
 * @brief Search execution status code.
 */
enum class SearchStatus : uint8_t {
    Success = 0,     // Search completed successfully
    EmptyQuery,      // Query was empty or contained only whitespace
    SyntaxError,     // Query parsing failed due to syntax or unsupported operator
    Cancelled,       // Search cancelled via cancellation token
    LimitExceeded,   // Query exceeded length, token count, or AST bounds
    InternalError    // Unhandled exception or internal error
};

/**
 * @brief Complete search response returned to callers.
 */
struct SearchResponse {
    std::string                             query;
    bool                                    isValid{false};
    std::string                             errorMessage;
    std::size_t                             errorOffset{0};
    SearchStatus                            status{SearchStatus::Success};
    std::size_t                             totalMatches{0};
    std::size_t                             returnedCount{0};
    std::size_t                             offset{0};
    std::size_t                             limit{0};
    dsa::CustomDynamicArray<DocID>          matchingDocIds;
    dsa::CustomDynamicArray<SearchResultItem> results;
    double                                  executionTimeMs{0.0};

    SearchResponse() = default;

    static SearchResponse makeError(SearchStatus stat,
                                    std::string queryStr,
                                    std::string err,
                                    std::size_t offset = 0) {
        SearchResponse resp;
        resp.query = std::move(queryStr);
        resp.isValid = false;
        resp.errorMessage = std::move(err);
        resp.errorOffset = offset;
        resp.status = stat;
        return resp;
    }
};

} // namespace sse::search
