#pragma once

#include <string>
#include "search/QueryToken.hpp"

namespace sse::ranking {

/**
 * @brief Represents a normalized positive query term to be evaluated during relevance ranking.
 */
struct ScoringTerm {
    std::string           term;
    search::FieldQualifier field{search::FieldQualifier::Unspecified};
    double                weight{1.0};

    ScoringTerm() = default;

    ScoringTerm(std::string t, search::FieldQualifier f = search::FieldQualifier::Unspecified, double w = 1.0)
        : term(std::move(t)), field(f), weight(w) {}

    bool operator==(const ScoringTerm& other) const noexcept {
        return term == other.term && field == other.field;
    }
};

} // namespace sse::ranking
