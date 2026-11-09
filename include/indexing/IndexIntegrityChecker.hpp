#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>
#include "indexing/InvertedIndex.hpp"
#include "indexing/DocumentCatalog.hpp"

namespace sse::indexing {

/**
 * @brief Diagnostic report capturing index invariants and integrity violations.
 */
struct IntegrityReport {
    bool                     isValid{true};
    std::size_t              errorCount{0};
    std::size_t              warningCount{0};
    std::vector<std::string> errors;
    std::size_t              totalTermsChecked{0};
    uint64_t                 totalPostingsChecked{0};

    void addError(std::string err) {
        isValid = false;
        ++errorCount;
        errors.push_back(std::move(err));
    }

    void addWarning(std::string warn) {
        ++warningCount;
        errors.push_back("[WARNING] " + std::move(warn));
    }

    std::string toString() const;
};

/**
 * @brief Diagnostic validator verifying all internal structural invariants of the InvertedIndex.
 */
class IndexIntegrityChecker {
public:
    /**
     * @brief Performs a deep non-mutating integrity audit of the InvertedIndex.
     * @param index Target InvertedIndex to validate.
     * @param catalog Optional DocumentCatalog to verify DocID bounds and active status.
     * @return Detailed diagnostic report.
     */
    static IntegrityReport checkIntegrity(const InvertedIndex& index,
                                          const DocumentCatalog* catalog = nullptr);
};

} // namespace sse::indexing
