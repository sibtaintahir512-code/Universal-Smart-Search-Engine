#include "TestHarness.hpp"
#include "persistence/IndexPersistenceManager.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include "search/SearchService.hpp"
#include <filesystem>
#include <vector>

namespace sse::test {

void runRoundTripEquivalenceTests() {
    std::cout << "[RUNNING] RoundTripEquivalence Tests..." << std::endl;

    using namespace sse::indexing;
    using namespace sse::search;
    using namespace sse::ranking;
    using namespace sse::persistence;

    std::filesystem::path testDir = "build/test_roundtrip_dir";
    std::error_code ec;
    std::filesystem::remove_all(testDir, ec);

    // Baseline container for query results before save
    struct QueryBaseline {
        std::string query;
        RankingMode mode;
        SearchResponse response;
    };

    std::vector<QueryBaseline> baselines;

    // Phase 1: Build in-memory search engine and index synthetic corpus
    {
        auto catalog = std::make_unique<DocumentCatalog>();
        auto index = std::make_unique<InvertedIndex>();

        // Register 5 documents
        FileMetadata m1{}; m1.fileSize = 1024; m1.category = FileCategory::Documents;
        DocID d1 = catalog->registerDocument(m1, "/docs/report_algorithm.pdf", "report_algorithm.pdf");

        FileMetadata m2{}; m2.fileSize = 2048; m2.category = FileCategory::SourceCode;
        DocID d2 = catalog->registerDocument(m2, "/src/compiler_core.cpp", "compiler_core.cpp");

        FileMetadata m3{}; m3.fileSize = 4096; m3.category = FileCategory::SourceCode;
        DocID d3 = catalog->registerDocument(m3, "/src/parser_ast.cpp", "parser_ast.cpp");

        FileMetadata m4{}; m4.fileSize = 512; m4.category = FileCategory::Documents;
        DocID d4 = catalog->registerDocument(m4, "/notes/deprecated_notes.txt", "deprecated_notes.txt");

        FileMetadata m5{}; m5.fileSize = 8192; m5.category = FileCategory::Documents;
        DocID d5 = catalog->registerDocument(m5, "/docs/performance_audit.pdf", "performance_audit.pdf");

        // Index terms with varied filename and body frequencies
        index->addTerm("algorithm", d1, 1, 6, FIELD_FILENAME | FIELD_CONTENT);
        index->addTerm("algorithm", d5, 0, 4, FIELD_CONTENT);

        index->addTerm("performance", d1, 0, 3, FIELD_CONTENT);
        index->addTerm("performance", d5, 1, 8, FIELD_FILENAME | FIELD_CONTENT);

        index->addTerm("compiler", d2, 1, 5, FIELD_FILENAME | FIELD_CONTENT);
        index->addTerm("parser", d3, 1, 4, FIELD_FILENAME | FIELD_CONTENT);
        index->addTerm("compiler", d3, 0, 2, FIELD_CONTENT);

        index->addTerm("deprecated", d4, 1, 2, FIELD_FILENAME | FIELD_CONTENT);
        index->addTerm("compiler", d4, 0, 1, FIELD_CONTENT);

        index->addTerm("data", d1, 0, 2, FIELD_CONTENT);
        index->addTerm("data", d2, 0, 2, FIELD_CONTENT);

        // Record length statistics for BM25
        index->documentLengthStatistics().recordDocumentLength(d1, 2, 80);
        index->documentLengthStatistics().recordDocumentLength(d2, 2, 150);
        index->documentLengthStatistics().recordDocumentLength(d3, 2, 120);
        index->documentLengthStatistics().recordDocumentLength(d4, 2, 40);
        index->documentLengthStatistics().recordDocumentLength(d5, 2, 200);

        // Queries to test across different ranking modes
        const std::vector<std::pair<std::string, RankingMode>> testQueries = {
            {"algorithm", RankingMode::BM25},
            {"algorithm", RankingMode::TFIDF},
            {"algorithm AND performance", RankingMode::BM25},
            {"compiler OR parser", RankingMode::BM25},
            {"compiler AND NOT deprecated", RankingMode::BM25},
            {"name:report AND data", RankingMode::BM25},
            {"performance", RankingMode::DocIdOrder}
        };

        for (const auto& tq : testQueries) {
            SearchConfig cfg;
            cfg.rankingConfig.mode = tq.second;
            cfg.rankingConfig.enableExplanation = true;
            SearchService service(index.get(), catalog.get(), cfg);

            SearchResponse resp = service.search(tq.first);
            TEST_ASSERT(resp.isValid);
            baselines.push_back({tq.first, tq.second, resp});
        }

        // Save index to disk
        IndexPersistenceManager persistMgr(index.get(), catalog.get(), testDir);
        PersistenceResult saveRes = persistMgr.saveIndex();
        TEST_ASSERT(saveRes.isSuccess());

        // DocumentCatalog and InvertedIndex are destroyed upon leaving this block
    }

    // Phase 2: Create completely fresh in-memory state and load from disk
    {
        auto freshCatalog = std::make_unique<DocumentCatalog>();
        auto freshIndex = std::make_unique<InvertedIndex>();

        TEST_ASSERT_EQ(freshCatalog->totalCount(), 0);
        TEST_ASSERT_EQ(freshIndex->vocabularySize(), 0);

        IndexPersistenceManager restoreMgr(freshIndex.get(), freshCatalog.get(), testDir);
        PersistenceResult loadRes = restoreMgr.loadIndex();
        TEST_ASSERT(loadRes.isSuccess());

        TEST_ASSERT_EQ(freshCatalog->totalCount(), 5);
        TEST_ASSERT_EQ(freshCatalog->activeCount(), 5);
        TEST_ASSERT(freshIndex->vocabularySize() >= 5);

        // Phase 3: Execute the same queries on restored engine and verify exact equivalence
        for (const auto& base : baselines) {
            SearchConfig cfg;
            cfg.rankingConfig.mode = base.mode;
            cfg.rankingConfig.enableExplanation = true;
            SearchService restoredService(freshIndex.get(), freshCatalog.get(), cfg);

            SearchResponse current = restoredService.search(base.query);

            TEST_ASSERT(current.isValid);
            TEST_ASSERT_EQ(current.totalMatches, base.response.totalMatches);
            TEST_ASSERT_EQ(current.results.size(), base.response.results.size());

            for (std::size_t i = 0; i < base.response.results.size(); ++i) {
                const auto& expectedItem = base.response.results[i];
                const auto& actualItem = current.results[i];

                // Exact DocID matching
                TEST_ASSERT_EQ(actualItem.docId, expectedItem.docId);

                // Exact Rank position matching
                TEST_ASSERT_EQ(actualItem.rankPosition, expectedItem.rankPosition);

                // Exact Relevance Score matching within floating point tolerance
                TEST_ASSERT_NEAR(actualItem.relevanceScore, expectedItem.relevanceScore, 1e-6);

                // Exact Metadata string matching
                TEST_ASSERT_EQ(actualItem.path, expectedItem.path);
                TEST_ASSERT_EQ(actualItem.filename, expectedItem.filename);
                TEST_ASSERT_EQ(actualItem.fileSize, expectedItem.fileSize);
            }
        }
    }

    // Cleanup test artifacts
    std::filesystem::remove_all(testDir, ec);
}

} // namespace sse::test
