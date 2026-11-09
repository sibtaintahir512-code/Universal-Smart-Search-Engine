#include "TestHarness.hpp"
#include <string>
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include "search/SearchService.hpp"

namespace sse::test {

void runSearchPaginationTests() {
    std::cout << "[RUNNING] SearchPagination Tests..." << std::endl;

    using namespace sse::indexing;
    using namespace sse::search;

    DocumentCatalog catalog;
    InvertedIndex index;

    // Register 15 documents containing the term "kernel"
    for (DocID id = 1; id <= 15; ++id) {
        FileMetadata meta;
        meta.category = FileCategory::SourceCode;
        meta.fileSize = 100 * id;
        meta.extractionState = ExtractionStatus::Extracted;

        std::string fname = "source_" + std::to_string(id) + ".cpp";
        std::string fpath = "/src/" + fname;
        catalog.registerDocument(meta, fpath, fname);

        index.addTerm("kernel", id, 0, 1, FIELD_CONTENT);
    }

    SearchService service(&index, &catalog);

    // 1. Pagination: Offset 0, Limit 5 (Page 1)
    {
        SearchResponse resp = service.search("kernel", 0, 5);
        TEST_ASSERT(resp.isValid);
        TEST_ASSERT(resp.status == SearchStatus::Success);
        TEST_ASSERT_EQ(resp.totalMatches, 15);
        TEST_ASSERT_EQ(resp.returnedCount, 5);
        TEST_ASSERT_EQ(resp.offset, 0);
        TEST_ASSERT_EQ(resp.limit, 5);
        TEST_ASSERT_EQ(resp.results.size(), 5);
        for (std::size_t i = 0; i < 5; ++i) {
            TEST_ASSERT_EQ(resp.matchingDocIds[i], static_cast<DocID>(i + 1));
            TEST_ASSERT_EQ(resp.results[i].docId, static_cast<DocID>(i + 1));
        }
    }

    // 2. Pagination: Offset 5, Limit 5 (Page 2)
    {
        SearchResponse resp = service.search("kernel", 5, 5);
        TEST_ASSERT(resp.isValid);
        TEST_ASSERT_EQ(resp.totalMatches, 15); // Total count unchanged
        TEST_ASSERT_EQ(resp.returnedCount, 5);
        TEST_ASSERT_EQ(resp.offset, 5);
        for (std::size_t i = 0; i < 5; ++i) {
            TEST_ASSERT_EQ(resp.matchingDocIds[i], static_cast<DocID>(i + 6));
            TEST_ASSERT_EQ(resp.results[i].docId, static_cast<DocID>(i + 6));
        }
    }

    // 3. Pagination: Offset 10, Limit 5 (Page 3 - Exact remaining)
    {
        SearchResponse resp = service.search("kernel", 10, 5);
        TEST_ASSERT(resp.isValid);
        TEST_ASSERT_EQ(resp.totalMatches, 15);
        TEST_ASSERT_EQ(resp.returnedCount, 5);
        TEST_ASSERT_EQ(resp.offset, 10);
        for (std::size_t i = 0; i < 5; ++i) {
            TEST_ASSERT_EQ(resp.matchingDocIds[i], static_cast<DocID>(i + 11));
            TEST_ASSERT_EQ(resp.results[i].docId, static_cast<DocID>(i + 11));
        }
    }

    // 4. Pagination: Offset 12, Limit 10 (Partial last page)
    {
        SearchResponse resp = service.search("kernel", 12, 10);
        TEST_ASSERT(resp.isValid);
        TEST_ASSERT_EQ(resp.totalMatches, 15);
        TEST_ASSERT_EQ(resp.returnedCount, 3); // Only docs 13, 14, 15 remain
        TEST_ASSERT_EQ(resp.results.size(), 3);
        TEST_ASSERT_EQ(resp.results[0].docId, 13);
        TEST_ASSERT_EQ(resp.results[1].docId, 14);
        TEST_ASSERT_EQ(resp.results[2].docId, 15);
    }

    // 5. Pagination: Offset Beyond Total Matches
    {
        SearchResponse resp = service.search("kernel", 20, 5);
        TEST_ASSERT(resp.isValid);
        TEST_ASSERT_EQ(resp.totalMatches, 15);
        TEST_ASSERT_EQ(resp.returnedCount, 0);
        TEST_ASSERT(resp.results.empty());
    }

    // 6. Pagination on Empty Query Matches
    {
        SearchResponse resp = service.search("nonexistent", 0, 10);
        TEST_ASSERT(resp.isValid);
        TEST_ASSERT_EQ(resp.totalMatches, 0);
        TEST_ASSERT_EQ(resp.returnedCount, 0);
        TEST_ASSERT(resp.results.empty());
    }
}

} // namespace sse::test
