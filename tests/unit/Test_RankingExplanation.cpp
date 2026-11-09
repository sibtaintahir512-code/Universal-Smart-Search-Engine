#include "TestHarness.hpp"
#include "search/SearchService.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"

namespace sse::test {

void runRankingExplanationTests() {
    std::cout << "[RUNNING] RankingExplanation Tests..." << std::endl;

    using namespace sse::ranking;
    using namespace sse::search;
    using namespace sse::indexing;

    DocumentCatalog catalog;
    InvertedIndex index;

    FileMetadata meta;
    meta.category = FileCategory::Documents;
    meta.extractionState = ExtractionStatus::Extracted;

    DocID id1 = catalog.registerDocument(meta, "/path/report.pdf", "report.pdf");
    index.documentLengthStatistics().recordDocumentLength(id1, 1, 100);
    index.addTerm("report", id1, 1, 4, FIELD_FILENAME | FIELD_CONTENT);

    // 1. Explanation Enabled
    {
        SearchConfig config;
        config.rankingConfig.mode = RankingMode::BM25;
        config.rankingConfig.enableExplanation = true;
        SearchService service(&index, &catalog, config);

        SearchResponse resp = service.search("report");
        TEST_ASSERT(resp.isValid);
        TEST_ASSERT_EQ(resp.results.size(), 1);

        const auto& item = resp.results[0];
        TEST_ASSERT(item.explanation != nullptr);
        TEST_ASSERT_EQ(item.explanation->docId, id1);
        TEST_ASSERT_NEAR(item.explanation->finalScore, item.relevanceScore, 1e-9);
        TEST_ASSERT(item.explanation->termDetails.size() == 1);

        const auto& termExpl = item.explanation->termDetails[0];
        TEST_ASSERT_EQ(termExpl.term, std::string("report"));
        TEST_ASSERT_EQ(termExpl.nameTf, 1);
        TEST_ASSERT_EQ(termExpl.bodyTf, 4);
        TEST_ASSERT(termExpl.nameScore > 0.0);
        TEST_ASSERT(termExpl.bodyScore > 0.0);
        TEST_ASSERT_NEAR(termExpl.totalTermScore, termExpl.nameScore + termExpl.bodyScore, 1e-9);
    }

    // 2. Explanation Disabled
    {
        SearchConfig config;
        config.rankingConfig.mode = RankingMode::BM25;
        config.rankingConfig.enableExplanation = false;
        SearchService service(&index, &catalog, config);

        SearchResponse resp = service.search("report");
        TEST_ASSERT(resp.isValid);
        TEST_ASSERT_EQ(resp.results.size(), 1);
        TEST_ASSERT(resp.results[0].explanation == nullptr);
    }
}

} // namespace sse::test
