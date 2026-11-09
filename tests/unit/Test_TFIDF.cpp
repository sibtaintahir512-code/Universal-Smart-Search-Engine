#include "TestHarness.hpp"
#include "ranking/TFIDFScorer.hpp"
#include "ranking/DocumentLengthStatistics.hpp"
#include "indexing/InvertedIndex.hpp"

namespace sse::test {

void runTFIDFTests() {
    std::cout << "[RUNNING] TFIDF Tests..." << std::endl;

    using namespace sse::ranking;
    using namespace sse::indexing;

    // 1. IDF Calculation Verification
    {
        // BM25-smoothed IDF: ln(1 + (N - DF + 0.5) / (DF + 0.5))
        double idf1 = TFIDFScorer::computeBM25SmoothedIdf(10, 1);
        double expected1 = std::log(1.0 + (10.0 - 1.0 + 0.5) / (1.0 + 0.5)); // ln(1 + 9.5/1.5) = ln(1 + 6.3333) = ln(7.3333)
        TEST_ASSERT_NEAR(idf1, expected1, 1e-6);

        // Rare term vs Common term
        double idfRare = TFIDFScorer::computeBM25SmoothedIdf(100, 2);
        double idfCommon = TFIDFScorer::computeBM25SmoothedIdf(100, 80);
        TEST_ASSERT(idfRare > idfCommon);

        // Edge cases: totalDocs = 0, DF = 0, DF > totalDocs
        TEST_ASSERT_NEAR(TFIDFScorer::computeBM25SmoothedIdf(0, 0), 0.0, 1e-9);
        TEST_ASSERT_NEAR(TFIDFScorer::computeBM25SmoothedIdf(10, 0), 0.0, 1e-9);
        TEST_ASSERT_NEAR(TFIDFScorer::computeBM25SmoothedIdf(10, 15), 0.0, 1e-9);

        // Standard smoothed IDF: ln((N + 1) / (DF + 1)) + 1.0
        double stdIdf = TFIDFScorer::computeConventionalIdf(10, 2);
        double expectedStd = std::log(11.0 / 3.0) + 1.0;
        TEST_ASSERT_NEAR(stdIdf, expectedStd, 1e-6);
        TEST_ASSERT_NEAR(TFIDFScorer::computeConventionalIdf(0, 0), 0.0, 1e-9);
    }

    // 2. Sublinear TF Scaling
    {
        // 1 + ln(1) = 1.0
        // 1 + ln(4) = 1 + 1.386 = 2.386
        // 1 + ln(16) = 1 + 2.772 = 3.772
        // Quadrupling TF less than doubles the sublinear TF value!
        double tf1 = 1.0 + std::log(1.0);
        double tf4 = 1.0 + std::log(4.0);
        double tf16 = 1.0 + std::log(16.0);
        TEST_ASSERT_NEAR(tf1, 1.0, 1e-9);
        TEST_ASSERT(tf4 > tf1);
        TEST_ASSERT(tf16 > tf4);
        TEST_ASSERT((tf16 - tf4) < (tf4 - tf1) * 2.0); // Sublinear growth
    }

    // 3. Document Scoring on InvertedIndex
    {
        InvertedIndex index;
        DocumentLengthStatistics stats;

        // Doc 1: "database" in filename (tf=1) and body (tf=3)
        // Doc 2: "database" in body only (tf=1)
        // Doc 3: "systems" in body only (tf=2)
        // Doc 4: "database" in body only (tf=4)
        stats.recordDocumentLength(1, 1, 10);
        stats.recordDocumentLength(2, 1, 10);
        stats.recordDocumentLength(3, 1, 10);
        stats.recordDocumentLength(4, 1, 10);

        index.addTerm("database", 1, 1, 3, FIELD_FILENAME | FIELD_CONTENT);
        index.addTerm("database", 2, 0, 1, FIELD_CONTENT);
        index.addTerm("systems", 3, 0, 2, FIELD_CONTENT);
        index.addTerm("database", 4, 0, 4, FIELD_CONTENT);

        RankingConfig config;
        config.mode = RankingMode::TFIDF;
        config.filenameWeight = 5.0;
        config.bodyWeight = 1.0;
        config.enableExplanation = true;

        dsa::CustomDynamicArray<ScoringTerm> terms;
        terms.push_back(ScoringTerm("database"));

        ScoreExplanation expl1;
        double score1 = TFIDFScorer::scoreDocument(1, terms, index, 4, config, &expl1);

        ScoreExplanation expl2;
        double score2 = TFIDFScorer::scoreDocument(2, terms, index, 4, config, &expl2);

        // Doc 1 has filename match and higher body TF => score1 > score2
        TEST_ASSERT(score1 > score2);
        TEST_ASSERT(score1 > 0.0);
        TEST_ASSERT(score2 > 0.0);

        // Verify explanations
        TEST_ASSERT_EQ(expl1.termDetails.size(), 1);
        TEST_ASSERT_EQ(expl1.termDetails[0].term, std::string("database"));
        TEST_ASSERT_EQ(expl1.termDetails[0].nameTf, 1);
        TEST_ASSERT_EQ(expl1.termDetails[0].bodyTf, 3);
        TEST_ASSERT_NEAR(expl1.finalScore, score1, 1e-6);

        TEST_ASSERT_EQ(expl2.termDetails.size(), 1);
        TEST_ASSERT_EQ(expl2.termDetails[0].term, std::string("database"));
        TEST_ASSERT_EQ(expl2.termDetails[0].nameTf, 0);
        TEST_ASSERT_EQ(expl2.termDetails[0].bodyTf, 1);

        // Doc 3 has no matches for "database" => score is 0
        ScoreExplanation expl3;
        double score3 = TFIDFScorer::scoreDocument(3, terms, index, 4, config, &expl3);
        TEST_ASSERT_NEAR(score3, 0.0, 1e-9);

        // Sublinear TF verification on scored documents:
        // Doc 2: bodyTf = 1 => 1.0 + ln(1) = 1.0
        // Doc 4: bodyTf = 4 => 1.0 + ln(4) = 2.386294
        // Ratio must be (1 + ln(4)) / 1.0 = ~2.3863, distinctly less than linear 4.0
        ScoreExplanation expl4;
        double score4 = TFIDFScorer::scoreDocument(4, terms, index, 4, config, &expl4);
        double expectedRatio = (1.0 + std::log(4.0)) / (1.0 + std::log(1.0));
        TEST_ASSERT_NEAR(score4 / score2, expectedRatio, 1e-5);
        TEST_ASSERT(score4 < score2 * 4.0); // Sublinear dampening proven
    }
}

} // namespace sse::test
