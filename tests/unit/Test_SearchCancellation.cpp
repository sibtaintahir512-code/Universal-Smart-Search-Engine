#include "TestHarness.hpp"
#include <string>
#include <atomic>
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include "search/SearchService.hpp"

namespace sse::test {

void runSearchCancellationTests() {
    std::cout << "[RUNNING] SearchCancellation & Limits Tests..." << std::endl;

    using namespace sse::indexing;
    using namespace sse::search;

    DocumentCatalog catalog;
    InvertedIndex index;

    for (DocID id = 1; id <= 10; ++id) {
        FileMetadata meta;
        meta.category = FileCategory::Documents;
        meta.fileSize = 1024;
        meta.extractionState = ExtractionStatus::Extracted;
        catalog.registerDocument(meta, "/docs/file" + std::to_string(id) + ".txt", "file" + std::to_string(id) + ".txt");
        index.addTerm("test", id, 0, 1, FIELD_CONTENT);
    }

    SearchService service(&index, &catalog);

    // 1. Pre-cancellation flag set before execution
    {
        std::atomic<bool> cancelToken{true};
        SearchResponse resp = service.search("test", 0, 10, &cancelToken);
        TEST_ASSERT(!resp.isValid);
        TEST_ASSERT(resp.status == SearchStatus::Cancelled);
        TEST_ASSERT(resp.errorMessage.find("cancelled") != std::string::npos);
        TEST_ASSERT(resp.results.empty());
    }

    // 2. Cancellation during active search
    {
        std::atomic<bool> cancelToken{false};
        cancelToken.store(true);
        SearchResponse resp = service.search("test OR test OR test", 0, 10, &cancelToken);
        TEST_ASSERT(!resp.isValid);
        TEST_ASSERT(resp.status == SearchStatus::Cancelled);
    }

    // 3. Exceeding Max Query Length
    {
        SearchConfig cfg;
        cfg.maxQueryLength = 15;
        SearchService limService(&index, &catalog, cfg);

        SearchResponse resp = limService.search("this query exceeds fifteen characters");
        TEST_ASSERT(!resp.isValid);
        TEST_ASSERT(resp.status == SearchStatus::SyntaxError);
        TEST_ASSERT(resp.errorMessage.find("exceeds maximum limit") != std::string::npos);
    }

    // 4. Exceeding Max Tokens
    {
        SearchConfig cfg;
        cfg.maxQueryTokens = 2;
        SearchService limService(&index, &catalog, cfg);

        SearchResponse resp = limService.search("one two three");
        TEST_ASSERT(!resp.isValid);
        TEST_ASSERT(resp.status == SearchStatus::SyntaxError);
        TEST_ASSERT(resp.errorMessage.find("maximum token limit") != std::string::npos);
    }

    // 5. Exceeding Max AST Depth
    {
        SearchConfig cfg;
        cfg.maxAstDepth = 2;
        SearchService limService(&index, &catalog, cfg);

        SearchResponse resp = limService.search("a AND b AND c");
        TEST_ASSERT(!resp.isValid);
        TEST_ASSERT(resp.status == SearchStatus::SyntaxError);
        TEST_ASSERT(resp.errorMessage.find("maximum AST depth") != std::string::npos);
    }

    // 6. Search on Empty Index
    {
        DocumentCatalog emptyCat;
        InvertedIndex emptyIdx;
        SearchService emptyService(&emptyIdx, &emptyCat);

        SearchResponse resp = emptyService.search("test");
        TEST_ASSERT(resp.isValid);
        TEST_ASSERT_EQ(resp.totalMatches, 0);
        TEST_ASSERT(resp.results.empty());
    }

    // 7. Search with Removed Documents (Tombstoned in catalog & pruned from index)
    {
        DocumentCatalog cat;
        InvertedIndex idx;

        FileMetadata meta1;
        meta1.extractionState = ExtractionStatus::Extracted;
        DocID id1 = cat.registerDocument(meta1, "/doc1.txt", "doc1.txt");
        idx.addTerm("active", id1, 0, 1, FIELD_CONTENT);

        FileMetadata meta2;
        meta2.extractionState = ExtractionStatus::Extracted;
        DocID id2 = cat.registerDocument(meta2, "/doc2.txt", "doc2.txt");
        idx.addTerm("active", id2, 0, 1, FIELD_CONTENT);

        SearchService svc(&idx, &cat);
        SearchResponse r1 = svc.search("active");
        TEST_ASSERT_EQ(r1.totalMatches, 2);

        // Delete doc2 from catalog and index
        cat.markDeleted(id2);
        idx.removeDocument(id2);

        SearchResponse r2 = svc.search("active");
        TEST_ASSERT_EQ(r2.totalMatches, 1);
        TEST_ASSERT_EQ(r2.matchingDocIds[0], id1);
    }
}

} // namespace sse::test
