#include "TestHarness.hpp"
#include "ranking/RankingStatistics.hpp"
#include <thread>
#include <vector>

namespace sse::test {

void runRankingStatisticsTests() {
    std::cout << "[RUNNING] RankingStatistics Tests..." << std::endl;

    using namespace sse::ranking;

    // 1. Initial State
    {
        RankingStatistics stats;
        TEST_ASSERT_EQ(stats.totalRankingRequests(), 0);
        TEST_ASSERT_EQ(stats.rankingFailures(), 0);
        TEST_ASSERT_EQ(stats.totalRankedDocuments(), 0);
        TEST_ASSERT_NEAR(stats.totalScoringDurationMs(), 0.0, 1e-9);
        TEST_ASSERT_NEAR(stats.totalTopKDurationMs(), 0.0, 1e-9);
        TEST_ASSERT_NEAR(stats.averageScoringDurationMs(), 0.0, 1e-9);
    }

    // 2. Record Ranking Runs
    {
        RankingStatistics stats;
        stats.recordRanking(RankingMode::BM25, 10, 2.5, 0.5, false);
        TEST_ASSERT_EQ(stats.totalRankingRequests(), 1);
        TEST_ASSERT_EQ(stats.rankingFailures(), 0);
        TEST_ASSERT_EQ(stats.bm25Requests(), 1);
        TEST_ASSERT_EQ(stats.totalRankedDocuments(), 10);
        TEST_ASSERT_NEAR(stats.totalScoringDurationMs(), 2.5, 1e-6);
        TEST_ASSERT_NEAR(stats.totalTopKDurationMs(), 0.5, 1e-6);
        TEST_ASSERT_NEAR(stats.averageScoringDurationMs(), 2.5, 1e-6);

        stats.recordRanking(RankingMode::TFIDF, 20, 3.5, 1.0, false);
        TEST_ASSERT_EQ(stats.totalRankingRequests(), 2);
        TEST_ASSERT_EQ(stats.rankingFailures(), 0);
        TEST_ASSERT_EQ(stats.tfidfRequests(), 1);
        TEST_ASSERT_EQ(stats.totalRankedDocuments(), 30);
        TEST_ASSERT_NEAR(stats.averageScoringDurationMs(), 3.0, 1e-6);

        // Record a failure
        stats.recordRanking(RankingMode::BM25, 0, 0.0, 0.0, true);
        TEST_ASSERT_EQ(stats.totalRankingRequests(), 3);
        TEST_ASSERT_EQ(stats.rankingFailures(), 1);

        // Reset
        stats.reset();
        TEST_ASSERT_EQ(stats.totalRankingRequests(), 0);
        TEST_ASSERT_EQ(stats.rankingFailures(), 0);
    }

    // 3. Concurrent Multi-threading
    {
        RankingStatistics stats;
        std::vector<std::thread> workers;
        for (int i = 0; i < 4; ++i) {
            workers.emplace_back([&stats]() {
                for (int j = 0; j < 50; ++j) {
                    stats.recordRanking(RankingMode::BM25, 5, 0.1, 0.05, false);
                }
            });
        }
        for (auto& w : workers) {
            w.join();
        }
        TEST_ASSERT_EQ(stats.totalRankingRequests(), 200);
        TEST_ASSERT_EQ(stats.totalRankedDocuments(), 1000);
    }
}

} // namespace sse::test
