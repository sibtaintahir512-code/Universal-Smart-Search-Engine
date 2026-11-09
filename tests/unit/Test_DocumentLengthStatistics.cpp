#include "TestHarness.hpp"
#include "ranking/DocumentLengthStatistics.hpp"
#include <thread>
#include <vector>

namespace sse::test {

void runDocumentLengthStatisticsTests() {
    std::cout << "[RUNNING] DocumentLengthStatistics Tests..." << std::endl;

    using namespace sse::ranking;

    // 1. Initial State
    {
        DocumentLengthStatistics stats;
        TEST_ASSERT_EQ(stats.activeDocumentCount(), 0);
        TEST_ASSERT_EQ(stats.totalNameTokens(), 0);
        TEST_ASSERT_EQ(stats.totalBodyTokens(), 0);
        TEST_ASSERT_NEAR(stats.averageNameLength(), 0.0, 1e-9);
        TEST_ASSERT_NEAR(stats.averageBodyLength(), 0.0, 1e-9);
        TEST_ASSERT_EQ(stats.getNameLength(1), 0);
        TEST_ASSERT_EQ(stats.getBodyLength(1), 0);
        TEST_ASSERT_EQ(stats.getTotalLength(1), 0);
        TEST_ASSERT(!stats.hasDocument(1));
    }

    // 2. Record Document Lengths
    {
        DocumentLengthStatistics stats;
        stats.recordDocumentLength(1, 3, 100);
        TEST_ASSERT(stats.hasDocument(1));
        TEST_ASSERT_EQ(stats.activeDocumentCount(), 1);
        TEST_ASSERT_EQ(stats.totalNameTokens(), 3);
        TEST_ASSERT_EQ(stats.totalBodyTokens(), 100);
        TEST_ASSERT_EQ(stats.getNameLength(1), 3);
        TEST_ASSERT_EQ(stats.getBodyLength(1), 100);
        TEST_ASSERT_EQ(stats.getTotalLength(1), 103);
        TEST_ASSERT_NEAR(stats.averageNameLength(), 3.0, 1e-9);
        TEST_ASSERT_NEAR(stats.averageBodyLength(), 100.0, 1e-9);

        // Add second doc
        stats.recordDocumentLength(2, 5, 200);
        TEST_ASSERT_EQ(stats.activeDocumentCount(), 2);
        TEST_ASSERT_EQ(stats.totalNameTokens(), 8);
        TEST_ASSERT_EQ(stats.totalBodyTokens(), 300);
        TEST_ASSERT_NEAR(stats.averageNameLength(), 4.0, 1e-9);
        TEST_ASSERT_NEAR(stats.averageBodyLength(), 150.0, 1e-9);

        // Re-recording doc 1 replaces previous values without double-counting
        stats.recordDocumentLength(1, 4, 150);
        TEST_ASSERT_EQ(stats.activeDocumentCount(), 2);
        TEST_ASSERT_EQ(stats.totalNameTokens(), 9); // 4 + 5
        TEST_ASSERT_EQ(stats.totalBodyTokens(), 350); // 150 + 200
        TEST_ASSERT_NEAR(stats.averageNameLength(), 4.5, 1e-9);
        TEST_ASSERT_NEAR(stats.averageBodyLength(), 175.0, 1e-9);
    }

    // 3. Accumulate Document Lengths
    {
        DocumentLengthStatistics stats;
        stats.accumulateDocumentLength(10, 2, 0);
        TEST_ASSERT(stats.hasDocument(10));
        TEST_ASSERT_EQ(stats.activeDocumentCount(), 1);
        TEST_ASSERT_EQ(stats.getNameLength(10), 2);
        TEST_ASSERT_EQ(stats.getBodyLength(10), 0);

        stats.accumulateDocumentLength(10, 0, 50);
        TEST_ASSERT_EQ(stats.getNameLength(10), 2);
        TEST_ASSERT_EQ(stats.getBodyLength(10), 50);
        TEST_ASSERT_EQ(stats.totalNameTokens(), 2);
        TEST_ASSERT_EQ(stats.totalBodyTokens(), 50);

        stats.accumulateDocumentLength(10, 0, 30);
        TEST_ASSERT_EQ(stats.getBodyLength(10), 80);
        TEST_ASSERT_EQ(stats.totalBodyTokens(), 80);
    }

    // 4. Remove Document & Clear
    {
        DocumentLengthStatistics stats;
        stats.recordDocumentLength(1, 2, 10);
        stats.recordDocumentLength(2, 4, 20);
        stats.recordDocumentLength(3, 6, 30);

        TEST_ASSERT_EQ(stats.activeDocumentCount(), 3);
        TEST_ASSERT(stats.removeDocument(2));
        TEST_ASSERT(!stats.hasDocument(2));
        TEST_ASSERT_EQ(stats.activeDocumentCount(), 2);
        TEST_ASSERT_EQ(stats.totalNameTokens(), 8); // 2 + 6
        TEST_ASSERT_EQ(stats.totalBodyTokens(), 40); // 10 + 30
        TEST_ASSERT_NEAR(stats.averageNameLength(), 4.0, 1e-9);
        TEST_ASSERT_NEAR(stats.averageBodyLength(), 20.0, 1e-9);

        // Removing non-existent doc returns false
        TEST_ASSERT(!stats.removeDocument(999));
        TEST_ASSERT_EQ(stats.activeDocumentCount(), 2);

        // Clear
        stats.clear();
        TEST_ASSERT_EQ(stats.activeDocumentCount(), 0);
        TEST_ASSERT_EQ(stats.totalNameTokens(), 0);
        TEST_ASSERT_EQ(stats.totalBodyTokens(), 0);
        TEST_ASSERT_NEAR(stats.averageNameLength(), 0.0, 1e-9);
        TEST_ASSERT_NEAR(stats.averageBodyLength(), 0.0, 1e-9);
    }

    // 5. Concurrent Access Test
    {
        DocumentLengthStatistics stats;
        std::vector<std::thread> threads;
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([&stats, t]() {
                for (uint32_t i = 1; i <= 50; ++i) {
                    DocID docId = static_cast<DocID>(static_cast<uint32_t>(t) * 100u + i);
                    stats.recordDocumentLength(docId, 2, 20);
                }
            });
        }
        for (auto& th : threads) {
            th.join();
        }
        TEST_ASSERT_EQ(stats.activeDocumentCount(), 200);
        TEST_ASSERT_EQ(stats.totalNameTokens(), 400);
        TEST_ASSERT_EQ(stats.totalBodyTokens(), 4000);
        TEST_ASSERT_NEAR(stats.averageNameLength(), 2.0, 1e-9);
        TEST_ASSERT_NEAR(stats.averageBodyLength(), 20.0, 1e-9);
    }
}

} // namespace sse::test
