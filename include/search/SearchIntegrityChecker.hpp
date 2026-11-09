#pragma once

#include <string>
#include "indexing/InvertedIndex.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "search/SearchService.hpp"
#include "dsa/CustomDynamicArray.hpp"

namespace sse::search {

/**
 * @brief Invariant validation report for search engine integrity.
 */
struct SearchIntegrityReport {
    bool passed{true};
    std::size_t checksRun{0};
    std::size_t checksPassed{0};
    dsa::CustomDynamicArray<std::string> violations;

    void addViolation(const std::string& msg) {
        passed = false;
        violations.push_back(msg);
    }

    void recordCheck(bool condition, const std::string& failureMsg) {
        ++checksRun;
        if (condition) {
            ++checksPassed;
        } else {
            addViolation(failureMsg);
        }
    }
};

/**
 * @brief Auditor verifying core algorithmic invariants of the search subsystem.
 */
class SearchIntegrityChecker {
public:
    /**
     * @brief Audits end-to-end invariants between the SearchService, InvertedIndex,
     * and DocumentCatalog.
     */
    static SearchIntegrityReport verifyInvariants(SearchService& service,
                                                 const indexing::InvertedIndex& index,
                                                 const indexing::DocumentCatalog& catalog);
};

} // namespace sse::search
