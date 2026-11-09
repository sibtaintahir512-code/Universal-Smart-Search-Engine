#include "indexing/IndexIntegrityChecker.hpp"
#include <sstream>

namespace sse::indexing {

std::string IntegrityReport::toString() const {
    std::ostringstream oss;
    oss << "Index Integrity Audit Report:\n"
        << "  Status               : " << (isValid ? "VALID (PASS)" : "INVALID (FAIL)") << "\n"
        << "  Terms Checked        : " << totalTermsChecked << "\n"
        << "  Postings Checked     : " << totalPostingsChecked << "\n"
        << "  Errors Detected      : " << errorCount << "\n"
        << "  Warnings Detected    : " << warningCount << "\n";
    if (!errors.empty()) {
        oss << "  Diagnostic Details:\n";
        for (const auto& err : errors) {
            oss << "    - " << err << "\n";
        }
    }
    return oss.str();
}

IntegrityReport IndexIntegrityChecker::checkIntegrity(const InvertedIndex& index,
                                                      const DocumentCatalog* catalog) {
    IntegrityReport report;
    uint64_t accumulatedPostings = 0;

    index.forEachTerm([&](const std::string& term, const dsa::CustomPostingList& list) {
        ++report.totalTermsChecked;

        // 1. Term string validity
        if (term.empty()) {
            report.addError("Index contains an empty string term key in hash table.");
            return;
        }

        // 2. Empty list retention check
        if (list.empty()) {
            report.addError("Term '" + term + "' has an empty posting list retained in index.");
            return;
        }

        // 3. Trie synchronization for eligible Latin [a-z0-9] terms
        if (dsa::CustomTrie::isValidAsciiWord(term)) {
            if (!index.trie().contains(term)) {
                report.addError("Term '" + term + "' exists in hash table but is missing from CustomTrie.");
            }
        }

        // 4. Verify posting chain invariants
        DocID prevDocId = INVALID_DOC_ID;
        uint32_t countedNodes = 0;
        const PostingNode* current = list.head();

        while (current) {
            ++countedNodes;
            ++accumulatedPostings;

            // Invariant A: DocID validity
            if (current->docId == INVALID_DOC_ID) {
                report.addError("Term '" + term + "' contains INVALID_DOC_ID (0) posting node.");
            } else if (catalog && current->docId > catalog->totalCount()) {
                report.addError("Term '" + term + "' references DocID " + std::to_string(current->docId) +
                                " exceeding catalog total count (" + std::to_string(catalog->totalCount()) + ").");
            }

            // Invariant B: Strict monotonic ascending DocID order (No duplicates)
            if (prevDocId != INVALID_DOC_ID) {
                if (current->docId == prevDocId) {
                    report.addError("Term '" + term + "' contains duplicate posting for DocID " +
                                    std::to_string(current->docId) + ".");
                } else if (current->docId < prevDocId) {
                    report.addError("Term '" + term + "' posting list is unsorted: DocID " +
                                    std::to_string(current->docId) + " follows " + std::to_string(prevDocId) + ".");
                }
            }

            // Invariant C: Term frequency non-zero validity
            if (current->nameTermFreq == 0 && current->bodyTermFreq == 0) {
                report.addError("Term '" + term + "' posting for DocID " + std::to_string(current->docId) +
                                " has both nameTermFreq and bodyTermFreq equal to zero.");
            }

            // Invariant D: Field bitmask consistency
            if (current->nameTermFreq > 0 && !(current->fieldMask & FIELD_FILENAME)) {
                report.addError("Term '" + term + "' DocID " + std::to_string(current->docId) +
                                " has nameTermFreq > 0 but FIELD_FILENAME bit is not set.");
            }
            if (current->bodyTermFreq > 0 && !(current->fieldMask & FIELD_CONTENT)) {
                report.addError("Term '" + term + "' DocID " + std::to_string(current->docId) +
                                " has bodyTermFreq > 0 but FIELD_CONTENT bit is not set.");
            }

            prevDocId = current->docId;
            current = current->next;
        }

        // Invariant E: Document frequency matches node count
        if (countedNodes != list.documentFrequency()) {
            report.addError("Term '" + term + "' declared documentFrequency (" +
                            std::to_string(list.documentFrequency()) + ") does not match counted nodes (" +
                            std::to_string(countedNodes) + ").");
        }
    });

    report.totalPostingsChecked = accumulatedPostings;

    // Invariant F: Index-wide total postings count agreement
    if (accumulatedPostings != index.totalPostingsCount()) {
        report.addError("Index totalPostingsCount (" + std::to_string(index.totalPostingsCount()) +
                        ") does not match accumulated postings (" + std::to_string(accumulatedPostings) + ").");
    }

    return report;
}

} // namespace sse::indexing
