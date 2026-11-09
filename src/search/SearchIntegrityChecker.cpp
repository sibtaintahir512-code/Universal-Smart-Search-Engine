#include "search/SearchIntegrityChecker.hpp"

namespace sse::search {

SearchIntegrityReport SearchIntegrityChecker::verifyInvariants(SearchService& service,
                                                             const indexing::InvertedIndex& index,
                                                             const indexing::DocumentCatalog& catalog) {
    SearchIntegrityReport report;

    // Snapshot pre-search index state to guarantee search never mutates index
    const std::size_t preVocabSize = index.vocabularySize();
    const uint64_t prePostings = index.totalPostingsCount();

    // Invariant 1: Empty query returns empty or syntax error with 0 matches
    SearchResponse emptyResp = service.search("");
    report.recordCheck(!emptyResp.isValid && emptyResp.status == SearchStatus::EmptyQuery,
                       "Empty query must fail validation with EmptyQuery status");

    // Invariant 2: Search does not mutate the Inverted Index
    report.recordCheck(index.vocabularySize() == preVocabSize,
                       "Index vocabulary size must remain unmodified after search");
    report.recordCheck(index.totalPostingsCount() == prePostings,
                       "Index total postings count must remain unmodified after search");

    // Invariant 3: Active catalog documents universe check
    std::size_t activeCount = catalog.activeCount();
    report.recordCheck(activeCount <= catalog.totalCount(),
                       "Active catalog document count must not exceed total registered count");

    return report;
}

} // namespace sse::search
