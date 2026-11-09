#include "TestHarness.hpp"
#include "ranking/TopKSelector.hpp"

namespace sse::test {

void runTopKSelectorTests() {
    std::cout << "[RUNNING] TopKSelector Tests..." << std::endl;

    using namespace sse::ranking;

    // 1. Basic Top-K Selection & Descending Order
    {
        dsa::CustomDynamicArray<ScoredDocument> docs;
        docs.push_back(ScoredDocument(10, 1.5));
        docs.push_back(ScoredDocument(20, 4.2));
        docs.push_back(ScoredDocument(30, 0.8));
        docs.push_back(ScoredDocument(40, 3.1));
        docs.push_back(ScoredDocument(50, 2.5));

        // Top 3
        auto top3 = TopKSelector::selectTopK(docs, 3);
        TEST_ASSERT_EQ(top3.size(), 3);
        TEST_ASSERT_EQ(top3[0].docId, 20); // 4.2
        TEST_ASSERT_EQ(top3[1].docId, 40); // 3.1
        TEST_ASSERT_EQ(top3[2].docId, 50); // 2.5
    }

    // 2. K = 0 (Select All Sorted)
    {
        dsa::CustomDynamicArray<ScoredDocument> docs;
        docs.push_back(ScoredDocument(1, 10.0));
        docs.push_back(ScoredDocument(2, 50.0));
        docs.push_back(ScoredDocument(3, 20.0));

        auto allSorted = TopKSelector::selectTopK(docs, 0);
        TEST_ASSERT_EQ(allSorted.size(), 3);
        TEST_ASSERT_EQ(allSorted[0].docId, 2);
        TEST_ASSERT_EQ(allSorted[1].docId, 3);
        TEST_ASSERT_EQ(allSorted[2].docId, 1);
    }

    // 3. K >= Total Candidates
    {
        dsa::CustomDynamicArray<ScoredDocument> docs;
        docs.push_back(ScoredDocument(100, 5.0));
        docs.push_back(ScoredDocument(200, 15.0));

        auto result = TopKSelector::selectTopK(docs, 10);
        TEST_ASSERT_EQ(result.size(), 2);
        TEST_ASSERT_EQ(result[0].docId, 200);
        TEST_ASSERT_EQ(result[1].docId, 100);
    }

    // 4. Deterministic Tie-Breaking (Equal score => lower DocID first)
    {
        dsa::CustomDynamicArray<ScoredDocument> docs;
        docs.push_back(ScoredDocument(40, 2.0));
        docs.push_back(ScoredDocument(10, 2.0));
        docs.push_back(ScoredDocument(30, 2.0));
        docs.push_back(ScoredDocument(20, 2.0));

        auto top2 = TopKSelector::selectTopK(docs, 2);
        TEST_ASSERT_EQ(top2.size(), 2);
        TEST_ASSERT_EQ(top2[0].docId, 10); // Lowest docId among tied scores
        TEST_ASSERT_EQ(top2[1].docId, 20);

        auto allTied = TopKSelector::selectTopK(docs, 4);
        TEST_ASSERT_EQ(allTied.size(), 4);
        TEST_ASSERT_EQ(allTied[0].docId, 10);
        TEST_ASSERT_EQ(allTied[1].docId, 20);
        TEST_ASSERT_EQ(allTied[2].docId, 30);
        TEST_ASSERT_EQ(allTied[3].docId, 40);
    }

    // 5. Empty Input
    {
        dsa::CustomDynamicArray<ScoredDocument> docs;
        auto res0 = TopKSelector::selectTopK(docs, 0);
        auto res5 = TopKSelector::selectTopK(docs, 5);
        TEST_ASSERT_EQ(res0.size(), 0);
        TEST_ASSERT_EQ(res5.size(), 0);
    }
}

} // namespace sse::test
