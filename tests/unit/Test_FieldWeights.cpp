#include "TestHarness.hpp"
#include "ranking/TFIDFScorer.hpp"
#include "ranking/BM25Scorer.hpp"
#include "ranking/DocumentLengthStatistics.hpp"
#include "indexing/InvertedIndex.hpp"

namespace sse::test {

void runFieldWeightsTests() {
    std::cout << "[RUNNING] FieldWeights Tests..." << std::endl;

    using namespace sse::ranking;
    using namespace sse::indexing;

    // 1. Config Validation
    {
        RankingConfig validConfig;
        TEST_ASSERT(validConfig.isValid());

        RankingConfig invalidNameWeight;
        invalidNameWeight.filenameWeight = -1.0;
        TEST_ASSERT(!invalidNameWeight.isValid());

        RankingConfig invalidBodyWeight;
        invalidBodyWeight.bodyWeight = -0.5;
        TEST_ASSERT(!invalidBodyWeight.isValid());

        RankingConfig invalidK1;
        invalidK1.bm25K1 = -0.1;
        TEST_ASSERT(!invalidK1.isValid());

        RankingConfig invalidB1;
        invalidB1.bm25B = -0.01;
        TEST_ASSERT(!invalidB1.isValid());

        RankingConfig invalidB2;
        invalidB2.bm25B = 1.05;
        TEST_ASSERT(!invalidB2.isValid());
    }

    // 2. Default Field Weights (W_name = 5.0, W_body = 1.0)
    {
        InvertedIndex index;
        DocumentLengthStatistics stats;

        // Doc 1: "compiler" in filename only (tf=1)
        // Doc 2: "compiler" in body only (tf=1)
        stats.recordDocumentLength(1, 1, 100);
        stats.recordDocumentLength(2, 1, 100);

        index.addTerm("compiler", 1, 1, 0, FIELD_FILENAME);
        index.addTerm("compiler", 2, 0, 1, FIELD_CONTENT);

        RankingConfig config;
        config.mode = RankingMode::TFIDF;
        config.filenameWeight = 5.0;
        config.bodyWeight = 1.0;

        dsa::CustomDynamicArray<ScoringTerm> terms;
        terms.push_back(ScoringTerm("compiler"));

        double score1 = TFIDFScorer::scoreDocument(1, terms, index, 2, config);
        double score2 = TFIDFScorer::scoreDocument(2, terms, index, 2, config);

        // Doc 1 has 5x weight of Doc 2
        TEST_ASSERT(score1 > score2);
        TEST_ASSERT_NEAR(score1, score2 * 5.0, 1e-6);
    }

    // 3. Field-Restricted Scoring Terms
    {
        InvertedIndex index;
        DocumentLengthStatistics stats;

        // Doc 1: "search" in filename only
        // Doc 2: "search" in body only
        stats.recordDocumentLength(1, 1, 100);
        stats.recordDocumentLength(2, 1, 100);

        index.addTerm("search", 1, 1, 0, FIELD_FILENAME);
        index.addTerm("search", 2, 0, 1, FIELD_CONTENT);

        RankingConfig config;
        config.mode = RankingMode::BM25;

        // Target field: Filename only
        dsa::CustomDynamicArray<ScoringTerm> nameTerms;
        nameTerms.push_back(ScoringTerm("search", sse::search::FieldQualifier::Filename));

        double nameScore1 = BM25Scorer::scoreDocument(1, nameTerms, index, stats, 2, config);
        double nameScore2 = BM25Scorer::scoreDocument(2, nameTerms, index, stats, 2, config);

        TEST_ASSERT(nameScore1 > 0.0);
        TEST_ASSERT_NEAR(nameScore2, 0.0, 1e-9);

        // Target field: Body only
        dsa::CustomDynamicArray<ScoringTerm> bodyTerms;
        bodyTerms.push_back(ScoringTerm("search", sse::search::FieldQualifier::Body));

        double bodyScore1 = BM25Scorer::scoreDocument(1, bodyTerms, index, stats, 2, config);
        double bodyScore2 = BM25Scorer::scoreDocument(2, bodyTerms, index, stats, 2, config);

        TEST_ASSERT_NEAR(bodyScore1, 0.0, 1e-9);
        TEST_ASSERT(bodyScore2 > 0.0);
    }
}

} // namespace sse::test
