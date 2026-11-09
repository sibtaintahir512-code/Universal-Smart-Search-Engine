#include "TestHarness.hpp"
#include "ranking/RankingEngine.hpp"
#include "search/QueryLexer.hpp"
#include "search/QueryParser.hpp"

namespace sse::test {

void runRankingDeterminismTests() {
    std::cout << "[RUNNING] RankingDeterminism Tests..." << std::endl;

    using namespace sse::ranking;
    using namespace sse::search;
    using namespace sse::indexing;

    DocumentCatalog catalog;
    InvertedIndex index;

    // Create 10 documents
    FileMetadata meta;
    meta.category = FileCategory::Documents;
    for (uint32_t i = 1; i <= 10; ++i) {
        DocID id = catalog.registerDocument(meta, "/path/doc" + std::to_string(i) + ".txt", "doc" + std::to_string(i) + ".txt");
        index.documentLengthStatistics().recordDocumentLength(id, 1, 50);
        // Distribute term frequencies
        uint16_t bodyTf = static_cast<uint16_t>((i % 3) + 1);
        uint16_t nameTf = static_cast<uint16_t>((i % 2 == 0) ? 1 : 0);
        uint8_t mask = FIELD_CONTENT | ((i % 2 == 0) ? FIELD_FILENAME : 0);
        index.addTerm("query", id, nameTf, bodyTf, mask);
    }

    ResultSet candidates;
    for (uint32_t i = 1; i <= 10; ++i) {
        candidates.add(i);
    }

    SearchConfig searchConfig;
    QueryLexer lexer(searchConfig);
    QueryParser parser(searchConfig);
    auto lexRes = lexer.tokenize("query");
    auto parseRes = parser.parse(lexRes.tokens);

    RankingConfig config;
    config.mode = RankingMode::BM25;
    RankingEngine engine(&index, &catalog, &index.documentLengthStatistics(), config);

    // Run 10 consecutive ranking calls
    auto firstRun = engine.rankCandidates(candidates, parseRes.root.get(), 10);
    TEST_ASSERT_EQ(firstRun.size(), 10);

    for (int run = 0; run < 10; ++run) {
        auto nextRun = engine.rankCandidates(candidates, parseRes.root.get(), 10);
        TEST_ASSERT_EQ(nextRun.size(), firstRun.size());
        for (std::size_t i = 0; i < firstRun.size(); ++i) {
            TEST_ASSERT_EQ(nextRun[i].docId, firstRun[i].docId);
            TEST_ASSERT_NEAR(nextRun[i].score, firstRun[i].score, 1e-12);
        }
    }
}

} // namespace sse::test
