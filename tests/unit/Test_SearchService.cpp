#include "TestHarness.hpp"
#include <string>
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include "search/SearchService.hpp"

namespace sse::test {

void runSearchServiceTests() {
    std::cout << "[RUNNING] SearchService Tests..." << std::endl;

    using namespace sse::indexing;
    using namespace sse::search;

    DocumentCatalog catalog;
    InvertedIndex index;

    // Doc 1
    FileMetadata meta1;
    meta1.category = FileCategory::Documents;
    meta1.fileSize = 4096;
    meta1.lastModifiedTime = 1600000000;
    meta1.extractionState = ExtractionStatus::Extracted;
    DocID id1 = catalog.registerDocument(meta1, "/home/user/notes.txt", "notes.txt");
    index.addTerm("quick", id1, 0, 1, FIELD_CONTENT);
    index.addTerm("brown", id1, 0, 1, FIELD_CONTENT);
    index.addTerm("fox", id1, 0, 1, FIELD_CONTENT);

    // Doc 2
    FileMetadata meta2;
    meta2.category = FileCategory::SourceCode;
    meta2.fileSize = 8192;
    meta2.lastModifiedTime = 1600000500;
    meta2.extractionState = ExtractionStatus::Extracted;
    DocID id2 = catalog.registerDocument(meta2, "/home/user/main.cpp", "main.cpp");
    index.addTerm("quick", id2, 0, 2, FIELD_CONTENT);
    index.addTerm("jumps", id2, 0, 1, FIELD_CONTENT);

    SearchService service(&index, &catalog);

    // 1. Successful Query & Result Metadata Resolution
    {
        SearchResponse resp = service.search("quick AND brown");
        TEST_ASSERT(resp.isValid);
        TEST_ASSERT(resp.status == SearchStatus::Success);
        TEST_ASSERT_EQ(resp.totalMatches, 1);
        TEST_ASSERT_EQ(resp.returnedCount, 1);
        TEST_ASSERT_EQ(resp.matchingDocIds[0], id1);

        const SearchResultItem& item = resp.results[0];
        TEST_ASSERT_EQ(item.docId, id1);
        TEST_ASSERT_EQ(item.filename, std::string("notes.txt"));
        TEST_ASSERT_EQ(item.path, std::string("/home/user/notes.txt"));
        TEST_ASSERT_EQ(item.extension, std::string(".txt"));
        TEST_ASSERT(item.category == FileCategory::Documents);
        TEST_ASSERT_EQ(item.fileSize, 4096);
        TEST_ASSERT_EQ(item.lastModified, 1600000000);
        TEST_ASSERT(item.hasExtractedContent == true);
    }

    // 2. Query with Multiple Matches
    {
        SearchResponse resp = service.search("quick");
        TEST_ASSERT(resp.isValid);
        TEST_ASSERT_EQ(resp.totalMatches, 2);
        TEST_ASSERT_EQ(resp.returnedCount, 2);
        TEST_ASSERT_EQ(resp.matchingDocIds[0], id1);
        TEST_ASSERT_EQ(resp.matchingDocIds[1], id2);
    }

    // 3. Query with Zero Matches
    {
        SearchResponse resp = service.search("nonexistent");
        TEST_ASSERT(resp.isValid);
        TEST_ASSERT_EQ(resp.totalMatches, 0);
        TEST_ASSERT_EQ(resp.returnedCount, 0);
        TEST_ASSERT(resp.results.empty());
    }

    // 4. Invalid Syntax Query
    {
        SearchResponse resp = service.search("quick AND");
        TEST_ASSERT(!resp.isValid);
        TEST_ASSERT(resp.status == SearchStatus::SyntaxError);
        TEST_ASSERT(!resp.errorMessage.empty());
    }

    // 5. Empty Query
    {
        SearchResponse resp = service.search("   ");
        TEST_ASSERT(!resp.isValid);
        TEST_ASSERT(resp.status == SearchStatus::EmptyQuery);
    }

    // 6. Cumulative Search Statistics Verification
    {
        const SearchStatistics& stats = service.statistics();
        TEST_ASSERT(stats.totalRequests() >= 5);
        TEST_ASSERT(stats.successfulSearches() >= 3);
        TEST_ASSERT(stats.invalidQueries() >= 2);
        TEST_ASSERT(stats.emptyResultSearches() >= 1);
        TEST_ASSERT(stats.totalMatchesReturned() >= 3);
        TEST_ASSERT(stats.termsEvaluated() > 0);
        TEST_ASSERT(stats.booleanOperationsEvaluated() > 0);
        TEST_ASSERT(stats.totalExecutionTimeMs() >= 0.0);
    }

    // 7. Reset Statistics
    {
        service.resetStatistics();
        const SearchStatistics& stats = service.statistics();
        TEST_ASSERT_EQ(stats.totalRequests(), 0);
        TEST_ASSERT_EQ(stats.successfulSearches(), 0);
        TEST_ASSERT_EQ(stats.invalidQueries(), 0);
    }
}

} // namespace sse::test
