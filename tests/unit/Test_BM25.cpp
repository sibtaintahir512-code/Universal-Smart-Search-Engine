#include "TestHarness.hpp"
#include "ranking/BM25Scorer.hpp"
#include "ranking/DocumentLengthStatistics.hpp"
#include "indexing/InvertedIndex.hpp"

namespace sse::test {

void runBM25Tests() {
    std::cout << "[RUNNING] BM25 Tests..." << std::endl;

    using namespace sse::ranking;
    using namespace sse::indexing;

    // 1. Length Normalization Test: Shorter doc gets higher score for same TF
    {
        InvertedIndex index;
        DocumentLengthStatistics stats;

        // Doc 1: short doc, body length 20, "search" tf = 2
        // Doc 2: long doc, body length 200, "search" tf = 2
        stats.recordDocumentLength(1, 1, 20);
        stats.recordDocumentLength(2, 1, 200);

        index.addTerm("search", 1, 0, 2, FIELD_CONTENT);
        index.addTerm("search", 2, 0, 2, FIELD_CONTENT);

        RankingConfig config;
        config.mode = RankingMode::BM25;
        config.bm25K1 = 1.2;
        config.bm25B = 0.75;
        config.enableExplanation = true;

        dsa::CustomDynamicArray<ScoringTerm> terms;
        terms.push_back(ScoringTerm("search"));

        ScoreExplanation expl1;
        double score1 = BM25Scorer::scoreDocument(1, terms, index, stats, 2, config, &expl1);

        ScoreExplanation expl2;
        double score2 = BM25Scorer::scoreDocument(2, terms, index, stats, 2, config, &expl2);

        TEST_ASSERT(score1 > score2);
        TEST_ASSERT(score1 > 0.0);
        TEST_ASSERT(score2 > 0.0);
    }

    // 2. Term Frequency Saturation: Diminishing returns with high TF
    {
        InvertedIndex index;
        DocumentLengthStatistics stats;

        // Same length docs
        stats.recordDocumentLength(1, 1, 500);
        stats.recordDocumentLength(2, 1, 500);
        stats.recordDocumentLength(3, 1, 500);

        // Doc 1: tf = 1
        // Doc 2: tf = 5
        // Doc 3: tf = 50
        index.addTerm("algorithm", 1, 0, 1, FIELD_CONTENT);
        index.addTerm("algorithm", 2, 0, 5, FIELD_CONTENT);
        index.addTerm("algorithm", 3, 0, 50, FIELD_CONTENT);

        RankingConfig config;
        config.mode = RankingMode::BM25;
        config.bm25K1 = 1.2;
        config.bm25B = 0.75;

        dsa::CustomDynamicArray<ScoringTerm> terms;
        terms.push_back(ScoringTerm("algorithm"));

        double score1 = BM25Scorer::scoreDocument(1, terms, index, stats, 3, config);
        double score2 = BM25Scorer::scoreDocument(2, terms, index, stats, 3, config);
        double score3 = BM25Scorer::scoreDocument(3, terms, index, stats, 3, config);

        TEST_ASSERT(score1 < score2);
        TEST_ASSERT(score2 < score3);

        // Difference between 1 and 5 should be significant; difference between 5 and 50 is bounded
        double diff12 = score2 - score1;
        double diff23 = score3 - score2;
        // Even though tf delta is 45 vs 4, saturation bounds diff23
        TEST_ASSERT(diff23 < diff12 * 2.5);
    }

    // 3. Parameter b Impact (b = 0 disables length penalty)
    {
        InvertedIndex index;
        DocumentLengthStatistics stats;

        // Doc 1: length 20, tf = 2
        // Doc 2: length 200, tf = 2
        stats.recordDocumentLength(1, 1, 20);
        stats.recordDocumentLength(2, 1, 200);

        index.addTerm("test", 1, 0, 2, FIELD_CONTENT);
        index.addTerm("test", 2, 0, 2, FIELD_CONTENT);

        RankingConfig config;
        config.mode = RankingMode::BM25;
        config.bm25K1 = 1.2;
        config.bm25B = 0.0; // Turn off length normalization

        dsa::CustomDynamicArray<ScoringTerm> terms;
        terms.push_back(ScoringTerm("test"));

        double score1 = BM25Scorer::scoreDocument(1, terms, index, stats, 2, config);
        double score2 = BM25Scorer::scoreDocument(2, terms, index, stats, 2, config);

        // With b = 0, both documents have identical score regardless of length!
        TEST_ASSERT_NEAR(score1, score2, 1e-6);
    }

    // 4. Filename-only documents guard (zero body tokens across corpus)
    {
        InvertedIndex index;
        DocumentLengthStatistics stats;

        stats.recordDocumentLength(1, 2, 0);
        stats.recordDocumentLength(2, 3, 0);

        index.addTerm("readme", 1, 1, 0, FIELD_FILENAME);
        index.addTerm("readme", 2, 1, 0, FIELD_FILENAME);

        RankingConfig config;
        config.mode = RankingMode::BM25;

        dsa::CustomDynamicArray<ScoringTerm> terms;
        terms.push_back(ScoringTerm("readme"));

        double score1 = BM25Scorer::scoreDocument(1, terms, index, stats, 2, config);
        double score2 = BM25Scorer::scoreDocument(2, terms, index, stats, 2, config);
        TEST_ASSERT(score1 > 0.0);
        TEST_ASSERT(score2 > 0.0);
        TEST_ASSERT(!std::isnan(score1));
        TEST_ASSERT(!std::isinf(score1));
    }
}

} // namespace sse::test
