#include "TestHarness.hpp"
#include "search/SearchService.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"

namespace sse::test {

void runRankingPaginationTests() {
    std::cout << "[RUNNING] RankingPagination Tests..." << std::endl;

    using namespace sse::ranking;
    using namespace sse::search;
    using namespace sse::indexing;

    DocumentCatalog catalog;
    InvertedIndex index;

    FileMetadata meta;
    meta.category = FileCategory::Documents;
    meta.extractionState = ExtractionStatus::Extracted;

    // Create 10 docs with distinct term frequencies
    for (uint32_t i = 1; i <= 10; ++i) {
        DocID id = catalog.registerDocument(meta, "/doc" + std::to_string(i) + ".txt", "doc" + std::to_string(i) + ".txt");
        index.documentLengthStatistics().recordDocumentLength(id, 1, 50);
        // Doc i has TF = i
        index.addTerm("test", id, 0, i, FIELD_CONTENT);
    }

    SearchConfig config;
    config.rankingConfig.mode = RankingMode::BM25;
    SearchService service(&index, &catalog, config);

    // Page 1: offset 0, limit 3
    SearchResponse page1 = service.search("test", 0, 3);
    TEST_ASSERT(page1.isValid);
    TEST_ASSERT_EQ(page1.totalMatches, 10);
    TEST_ASSERT_EQ(page1.returnedCount, 3);
    TEST_ASSERT_EQ(page1.results.size(), 3);
    TEST_ASSERT_EQ(page1.results[0].rankPosition, 1);
    TEST_ASSERT_EQ(page1.results[1].rankPosition, 2);
    TEST_ASSERT_EQ(page1.results[2].rankPosition, 3);

    // Highest TF was doc 10, then 9, then 8
    TEST_ASSERT_EQ(page1.results[0].docId, 10);
    TEST_ASSERT_EQ(page1.results[1].docId, 9);
    TEST_ASSERT_EQ(page1.results[2].docId, 8);

    // Page 2: offset 3, limit 3
    SearchResponse page2 = service.search("test", 3, 3);
    TEST_ASSERT(page2.isValid);
    TEST_ASSERT_EQ(page2.totalMatches, 10);
    TEST_ASSERT_EQ(page2.returnedCount, 3);
    TEST_ASSERT_EQ(page2.results[0].rankPosition, 4);
    TEST_ASSERT_EQ(page2.results[1].rankPosition, 5);
    TEST_ASSERT_EQ(page2.results[2].rankPosition, 6);

    TEST_ASSERT_EQ(page2.results[0].docId, 7);
    TEST_ASSERT_EQ(page2.results[1].docId, 6);
    TEST_ASSERT_EQ(page2.results[2].docId, 5);

    // Cross-page descending order
    TEST_ASSERT(page1.results[2].relevanceScore > page2.results[0].relevanceScore);

    // Out-of-bounds offset
    SearchResponse emptyPage = service.search("test", 20, 5);
    TEST_ASSERT(emptyPage.isValid);
    TEST_ASSERT_EQ(emptyPage.totalMatches, 10);
    TEST_ASSERT_EQ(emptyPage.returnedCount, 0);
    TEST_ASSERT_EQ(emptyPage.results.size(), 0);
}

} // namespace sse::test
