#include "TestHarness.hpp"
#include "ranking/RankingEngine.hpp"
#include "search/QueryLexer.hpp"
#include "search/QueryParser.hpp"
#include "search/SearchService.hpp"
#include <chrono>

namespace sse::test {

void runPhase7BenchmarkTests() {
    std::cout << "[RUNNING] Phase 7 Relevance Ranking Benchmarks..." << std::endl;

    using namespace sse::ranking;
    using namespace sse::search;
    using namespace sse::indexing;

    DocumentCatalog catalog;
    InvertedIndex index;

    FileMetadata meta;
    meta.category = FileCategory::Documents;
    meta.extractionState = ExtractionStatus::Extracted;

    const uint32_t CORPUS_SIZE = 10000;
    std::cout << "  Generating synthetic corpus of " << CORPUS_SIZE << " documents..." << std::endl;

    for (uint32_t i = 1; i <= CORPUS_SIZE; ++i) {
        DocID id = catalog.registerDocument(meta, "/path/doc" + std::to_string(i) + ".txt", "doc" + std::to_string(i) + ".txt");
        uint32_t bodyLen = 50 + (i % 200);
        index.documentLengthStatistics().recordDocumentLength(id, 1, bodyLen);

        // Every doc has "common"
        uint16_t commonBody = static_cast<uint16_t>((i % 5) + 1);
        uint16_t commonName = static_cast<uint16_t>((i % 50 == 0) ? 1 : 0);
        uint8_t commonMask = FIELD_CONTENT | ((i % 50 == 0) ? FIELD_FILENAME : 0);
        index.addTerm("common", id, commonName, commonBody, commonMask);

        // 1 in 10 docs has "medium"
        if (i % 10 == 0) {
            index.addTerm("medium", id, 0, static_cast<uint16_t>((i % 3) + 1), FIELD_CONTENT);
        }

        // 1 in 100 docs has "rare"
        if (i % 100 == 0) {
            index.addTerm("rare", id, 0, 1, FIELD_CONTENT);
        }
    }

    SearchConfig baseConfig;

    // Benchmark 1: DocIdOrder (Unranked Baseline)
    {
        baseConfig.rankingConfig.mode = RankingMode::DocIdOrder;
        SearchService service(&index, &catalog, baseConfig);

        auto t0 = std::chrono::high_resolution_clock::now();
        SearchResponse resp = service.search("common", 0, 20);
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(t1 - t0).count();

        TEST_ASSERT(resp.isValid);
        TEST_ASSERT_EQ(resp.totalMatches, CORPUS_SIZE);
        TEST_ASSERT_EQ(resp.returnedCount, 20);
        std::cout << "  [Benchmark] DocIdOrder (10k docs, top 20): " << elapsed << " ms" << std::endl;
        TEST_ASSERT(elapsed < 200.0); // Generous ceiling
    }

    // Benchmark 2: TF-IDF Ranking (10k docs, Top-20 selection)
    {
        baseConfig.rankingConfig.mode = RankingMode::TFIDF;
        SearchService service(&index, &catalog, baseConfig);

        auto t0 = std::chrono::high_resolution_clock::now();
        SearchResponse resp = service.search("common", 0, 20);
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(t1 - t0).count();

        TEST_ASSERT(resp.isValid);
        TEST_ASSERT_EQ(resp.totalMatches, CORPUS_SIZE);
        TEST_ASSERT_EQ(resp.returnedCount, 20);
        // Documents with filename match should be at the top!
        TEST_ASSERT(resp.results[0].relevanceScore > 0.0);
        std::cout << "  [Benchmark] TF-IDF (10k docs, top 20): " << elapsed << " ms" << std::endl;
        TEST_ASSERT(elapsed < 500.0);
    }

    // Benchmark 3: BM25 Ranking (10k docs, Top-20 selection)
    {
        baseConfig.rankingConfig.mode = RankingMode::BM25;
        SearchService service(&index, &catalog, baseConfig);

        auto t0 = std::chrono::high_resolution_clock::now();
        SearchResponse resp = service.search("common", 0, 20);
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(t1 - t0).count();

        TEST_ASSERT(resp.isValid);
        TEST_ASSERT_EQ(resp.totalMatches, CORPUS_SIZE);
        TEST_ASSERT_EQ(resp.returnedCount, 20);
        TEST_ASSERT(resp.results[0].relevanceScore > 0.0);
        std::cout << "  [Benchmark] BM25 (10k docs, top 20): " << elapsed << " ms" << std::endl;
        TEST_ASSERT(elapsed < 500.0);
    }

    // Benchmark 4: Multi-term ranking: "common AND medium"
    {
        baseConfig.rankingConfig.mode = RankingMode::BM25;
        SearchService service(&index, &catalog, baseConfig);

        auto t0 = std::chrono::high_resolution_clock::now();
        SearchResponse resp = service.search("common AND medium", 0, 20);
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(t1 - t0).count();

        TEST_ASSERT(resp.isValid);
        TEST_ASSERT_EQ(resp.totalMatches, 1000);
        TEST_ASSERT_EQ(resp.returnedCount, 20);
        std::cout << "  [Benchmark] BM25 Multi-term 'common AND medium' (1k matches): " << elapsed << " ms" << std::endl;
        TEST_ASSERT(elapsed < 200.0);
    }
}

} // namespace sse::test
