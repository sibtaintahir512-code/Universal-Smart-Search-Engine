#include "indexing/IndexingStatistics.hpp"
#include <sstream>
#include <iomanip>

namespace sse::indexing {

std::string IndexingStatistics::toString() const {
    std::ostringstream oss;
    oss << "Indexing Statistics:\n"
        << "  Documents Processed         : " << documentsProcessed << "\n"
        << "  Documents Fully Indexed     : " << documentsFullyIndexed << "\n"
        << "  Documents Partially Indexed : " << documentsPartiallyIndexed << "\n"
        << "  Documents Filename-Only     : " << documentsFilenameOnly << "\n"
        << "  Documents Failed            : " << documentsFailed << "\n"
        << "  Total Tokens Processed      : " << totalTokensProcessed << "\n"
        << "  Unique Vocabulary Count     : " << uniqueVocabularyCount << "\n"
        << "  Total Postings Count        : " << totalPostingsCount << "\n"
        << "  Body Term Occurrences       : " << totalBodyTermOccurrences << "\n"
        << "  Filename Term Occurrences   : " << totalFilenameTermOccurrences << "\n"
        << "  Average Tokens/Document     : " << std::fixed << std::setprecision(2) << averageTokensPerDocument() << "\n"
        << "  Total Indexing Duration     : " << std::fixed << std::setprecision(2) << indexingDurationMs << " ms\n";
    return oss.str();
}

} // namespace sse::indexing
