#include "TestHarness.hpp"
#include "ranking/RankingEngine.hpp"
#include "search/QueryLexer.hpp"
#include "search/QueryParser.hpp"
#include "search/QueryEvaluator.hpp"

namespace sse::test {

void runRankingEngineTests() {
    std::cout << "[RUNNING] RankingEngine Tests..." << std::endl;

    using namespace sse::ranking;
    using namespace sse::search;
    using namespace sse::indexing;

    // 1. Positive Query Term Extraction
    {
        SearchConfig config;
        QueryLexer lexer(config);
        QueryParser parser(config);

        // Expression: (apple OR banana) AND NOT cherry
        auto lexRes = lexer.tokenize("(apple OR banana) AND NOT cherry");
        auto parseRes = parser.parse(lexRes.tokens);
        TEST_ASSERT(parseRes.success);

        auto terms = RankingEngine::extractPositiveQueryTerms(parseRes.root.get());
        TEST_ASSERT_EQ(terms.size(), 2);
        TEST_ASSERT_EQ(terms[0].term, std::string("apple"));
        TEST_ASSERT_EQ(terms[1].term, std::string("banana"));
    }

    // 2. Pure Negation Query Term Extraction
    {
        SearchConfig config;
        QueryLexer lexer(config);
        QueryParser parser(config);

        auto lexRes = lexer.tokenize("NOT test");
        auto parseRes = parser.parse(lexRes.tokens);
        TEST_ASSERT(parseRes.success);

        auto terms = RankingEngine::extractPositiveQueryTerms(parseRes.root.get());
        TEST_ASSERT_EQ(terms.size(), 0); // Excluded!
    }

    // 3. Ranking Modes on Sample Corpus
    {
        DocumentCatalog catalog;
        InvertedIndex index;

        // Doc 1: "engine" in body tf=1
        // Doc 2: "engine" in name tf=1, body tf=2
        // Doc 3: "engine" in body tf=5
        FileMetadata meta;
        meta.category = FileCategory::Documents;
        DocID id1 = catalog.registerDocument(meta, "/path/doc1.txt", "doc1.txt");
        DocID id2 = catalog.registerDocument(meta, "/path/engine.txt", "engine.txt");
        DocID id3 = catalog.registerDocument(meta, "/path/doc3.txt", "doc3.txt");

        index.documentLengthStatistics().recordDocumentLength(id1, 1, 20);
        index.documentLengthStatistics().recordDocumentLength(id2, 1, 20);
        index.documentLengthStatistics().recordDocumentLength(id3, 1, 20);

        index.addTerm("engine", id1, 0, 1, FIELD_CONTENT);
        index.addTerm("engine", id2, 1, 2, FIELD_FILENAME | FIELD_CONTENT);
        index.addTerm("engine", id3, 0, 5, FIELD_CONTENT);

        ResultSet candidates;
        candidates.add(id1);
        candidates.add(id2);
        candidates.add(id3);

        SearchConfig searchConfig;
        QueryLexer lexer(searchConfig);
        QueryParser parser(searchConfig);
        auto lexRes = lexer.tokenize("engine");
        auto parseRes = parser.parse(lexRes.tokens);

        // DocIdOrder
        {
            RankingConfig cfg;
            cfg.mode = RankingMode::DocIdOrder;
            RankingEngine engine(&index, &catalog, &index.documentLengthStatistics(), cfg);
            auto ranked = engine.rankCandidates(candidates, parseRes.root.get(), 0);
            TEST_ASSERT_EQ(ranked.size(), 3);
            TEST_ASSERT_EQ(ranked[0].docId, id1);
            TEST_ASSERT_EQ(ranked[1].docId, id2);
            TEST_ASSERT_EQ(ranked[2].docId, id3);
        }

        // BM25 Ranking
        {
            RankingConfig cfg;
            cfg.mode = RankingMode::BM25;
            RankingEngine engine(&index, &catalog, &index.documentLengthStatistics(), cfg);
            auto ranked = engine.rankCandidates(candidates, parseRes.root.get(), 0);
            TEST_ASSERT_EQ(ranked.size(), 3);
            // Doc 2 has filename match (5.0 weight), so it ranks #1
            TEST_ASSERT_EQ(ranked[0].docId, id2);
            // Doc 3 has tf=5 vs Doc 1 tf=1, so Doc 3 ranks #2
            TEST_ASSERT_EQ(ranked[1].docId, id3);
            // Doc 1 ranks #3
            TEST_ASSERT_EQ(ranked[2].docId, id1);
        }
    }
}

} // namespace sse::test
