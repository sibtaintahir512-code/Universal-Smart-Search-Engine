#include "TestHarness.hpp"
#include "search/SearchService.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"

namespace sse::test {

void runBooleanAwareRankingTests() {
    std::cout << "[RUNNING] BooleanAwareRanking Tests..." << std::endl;

    using namespace sse::ranking;
    using namespace sse::search;
    using namespace sse::indexing;

    DocumentCatalog catalog;
    InvertedIndex index;

    FileMetadata meta;
    meta.category = FileCategory::Documents;
    meta.extractionState = ExtractionStatus::Extracted;

    // Doc 1: "apple", "banana"
    DocID id1 = catalog.registerDocument(meta, "/doc1.txt", "doc1.txt");
    index.documentLengthStatistics().recordDocumentLength(id1, 1, 50);
    index.addTerm("apple", id1, 0, 2, FIELD_CONTENT);
    index.addTerm("banana", id1, 0, 1, FIELD_CONTENT);

    // Doc 2: "apple" only
    DocID id2 = catalog.registerDocument(meta, "/doc2.txt", "doc2.txt");
    index.documentLengthStatistics().recordDocumentLength(id2, 1, 50);
    index.addTerm("apple", id2, 0, 2, FIELD_CONTENT);

    // Doc 3: "apple", "cherry"
    DocID id3 = catalog.registerDocument(meta, "/doc3.txt", "doc3.txt");
    index.documentLengthStatistics().recordDocumentLength(id3, 1, 50);
    index.addTerm("apple", id3, 0, 2, FIELD_CONTENT);
    index.addTerm("cherry", id3, 0, 5, FIELD_CONTENT);

    SearchConfig config;
    config.rankingConfig.mode = RankingMode::BM25;
    SearchService service(&index, &catalog, config);

    // 1. OR Query: "apple OR banana"
    // Doc 1 has both apple and banana -> ranks #1!
    {
        SearchResponse resp = service.search("apple OR banana");
        TEST_ASSERT(resp.isValid);
        TEST_ASSERT_EQ(resp.totalMatches, 3);
        TEST_ASSERT_EQ(resp.results.size(), 3);
        TEST_ASSERT_EQ(resp.results[0].docId, id1);
        TEST_ASSERT(resp.results[0].relevanceScore > resp.results[1].relevanceScore);
    }

    // 2. AND NOT Query: "apple AND NOT cherry"
    // Doc 3 must be completely excluded! Doc 1 and Doc 2 match.
    // Cherry must NOT contribute any positive score.
    {
        SearchResponse resp = service.search("apple AND NOT cherry");
        TEST_ASSERT(resp.isValid);
        TEST_ASSERT_EQ(resp.totalMatches, 2);
        TEST_ASSERT_EQ(resp.results.size(), 2);
        for (const auto& r : resp.results) {
            TEST_ASSERT(r.docId != id3);
        }
    }

    // 3. Pure NOT Query: "NOT cherry"
    // Doc 1 and Doc 2 match. Scoring terms are empty.
    // Scores must be 0.0, sorted by ascending DocID!
    {
        SearchResponse resp = service.search("NOT cherry");
        TEST_ASSERT(resp.isValid);
        TEST_ASSERT_EQ(resp.totalMatches, 2);
        TEST_ASSERT_EQ(resp.results.size(), 2);
        TEST_ASSERT_EQ(resp.results[0].docId, id1);
        TEST_ASSERT_EQ(resp.results[1].docId, id2);
        TEST_ASSERT_NEAR(resp.results[0].relevanceScore, 0.0, 1e-9);
        TEST_ASSERT_NEAR(resp.results[1].relevanceScore, 0.0, 1e-9);
    }
}

} // namespace sse::test
