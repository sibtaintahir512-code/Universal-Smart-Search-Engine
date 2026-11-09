#include "TestHarness.hpp"
#include "ranking/RankingEngine.hpp"
#include "search/QueryLexer.hpp"
#include "search/QueryParser.hpp"

namespace sse::test {

void runRankingUpdatesTests() {
    std::cout << "[RUNNING] RankingUpdates Tests..." << std::endl;

    using namespace sse::ranking;
    using namespace sse::search;
    using namespace sse::indexing;

    DocumentCatalog catalog;
    InvertedIndex index;

    FileMetadata meta;
    meta.category = FileCategory::Documents;

    DocID id1 = catalog.registerDocument(meta, "/path/doc1.txt", "doc1.txt");
    DocID id2 = catalog.registerDocument(meta, "/path/doc2.txt", "doc2.txt");

    index.documentLengthStatistics().recordDocumentLength(id1, 1, 100);
    index.documentLengthStatistics().recordDocumentLength(id2, 1, 100);

    index.addTerm("common", id1, 0, 1, FIELD_CONTENT);
    index.addTerm("common", id2, 0, 1, FIELD_CONTENT);

    index.addTerm("rare", id1, 0, 1, FIELD_CONTENT);

    SearchConfig searchConfig;
    QueryLexer lexer(searchConfig);
    QueryParser parser(searchConfig);
    auto lexRes = lexer.tokenize("rare");
    auto parseRes = parser.parse(lexRes.tokens);

    RankingConfig config;
    config.mode = RankingMode::TFIDF;
    RankingEngine engine(&index, &catalog, &index.documentLengthStatistics(), config);

    ResultSet candidates;
    candidates.add(id1);

    auto rank1 = engine.rankCandidates(candidates, parseRes.root.get(), 0);
    TEST_ASSERT_EQ(rank1.size(), 1);
    double initialScore = rank1[0].score;
    TEST_ASSERT(initialScore > 0.0);

    // Now index "rare" into 10 new documents, making it common!
    for (uint32_t i = 3; i <= 12; ++i) {
        DocID id = catalog.registerDocument(meta, "/path/doc" + std::to_string(i) + ".txt", "doc" + std::to_string(i) + ".txt");
        index.documentLengthStatistics().recordDocumentLength(id, 1, 100);
        index.addTerm("rare", id, 0, 1, FIELD_CONTENT);
    }

    // Now DF of "rare" increased from 1 to 11.
    // InvertedIndex total docs is 12.
    // The IDF of "rare" should decrease significantly.
    auto rank2 = engine.rankCandidates(candidates, parseRes.root.get(), 0);
    TEST_ASSERT_EQ(rank2.size(), 1);
    TEST_ASSERT(rank2[0].score < initialScore);

    // Remove document 1 from index
    index.removeDocument(id1);
    TEST_ASSERT(!index.documentLengthStatistics().hasDocument(id1));
}

} // namespace sse::test
