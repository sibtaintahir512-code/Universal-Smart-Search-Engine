#include "TestHarness.hpp"
#include "dsa/CustomMaxHeap.hpp"
#include "core/CommonTypes.hpp"

namespace sse::test {

void runMaxHeapTests() {
    std::cout << "[RUNNING] CustomMaxHeap Tests..." << std::endl;

    // Test 1: Empty state & underflow
    {
        CustomMaxHeap<int> heap;
        TEST_ASSERT(heap.empty());
        TEST_ASSERT_EQ(heap.size(), 0);
        TEST_ASSERT_THROWS(heap.top(), std::underflow_error);
        TEST_ASSERT_THROWS(heap.pop(), std::underflow_error);
    }

    // Test 2: Push & Descending Order Extraction
    {
        CustomMaxHeap<int> heap;
        heap.push(10);
        heap.push(30);
        heap.push(20);
        heap.push(5);
        heap.push(50);

        TEST_ASSERT_EQ(heap.size(), 5);
        TEST_ASSERT_EQ(heap.top(), 50);

        TEST_ASSERT_EQ(heap.extractTop(), 50);
        TEST_ASSERT_EQ(heap.extractTop(), 30);
        TEST_ASSERT_EQ(heap.extractTop(), 20);
        TEST_ASSERT_EQ(heap.extractTop(), 10);
        TEST_ASSERT_EQ(heap.extractTop(), 5);
        TEST_ASSERT(heap.empty());
    }

    // Test 3: Floyd's Linear O(N) Build Heap
    {
        CustomDynamicArray<int> data;
        data.push_back(3);
        data.push_back(10);
        data.push_back(12);
        data.push_back(8);
        data.push_back(2);
        data.push_back(14);

        CustomMaxHeap<int> heap(std::move(data));
        TEST_ASSERT_EQ(heap.size(), 6);
        TEST_ASSERT_EQ(heap.extractTop(), 14);
        TEST_ASSERT_EQ(heap.extractTop(), 12);
        TEST_ASSERT_EQ(heap.extractTop(), 10);
        TEST_ASSERT_EQ(heap.extractTop(), 8);
        TEST_ASSERT_EQ(heap.extractTop(), 3);
        TEST_ASSERT_EQ(heap.extractTop(), 2);
    }

    // Test 4: Replace Top O(log N)
    {
        CustomMaxHeap<int> heap;
        heap.push(100);
        heap.push(50);
        heap.push(80);

        TEST_ASSERT_EQ(heap.top(), 100);
        heap.replaceTop(60); // 100 replaced by 60 -> root becomes 80
        TEST_ASSERT_EQ(heap.top(), 80);
    }

    // Test 5: RankingEngine HeapNode Tie-Breaking Invariant
    {
        CustomMaxHeap<HeapNode> heap;

        HeapNode n1{1, 5.0f, 1000}; // Doc 1: score 5.0
        HeapNode n2{2, 9.5f, 1000}; // Doc 2: score 9.5 (highest score)
        HeapNode n3{3, 5.0f, 2000}; // Doc 3: score 5.0, newer timestamp (beats Doc 1)
        HeapNode n4{4, 5.0f, 2000}; // Doc 4: score 5.0, same timestamp as Doc 3, but docId 4 > 3 (Doc 3 wins tie-breaker)

        heap.push(n1);
        heap.push(n2);
        heap.push(n3);
        heap.push(n4);

        // Expected extraction order:
        // 1. Doc 2 (Score 9.5)
        // 2. Doc 3 (Score 5.0, ts 2000, id 3)
        // 3. Doc 4 (Score 5.0, ts 2000, id 4)
        // 4. Doc 1 (Score 5.0, ts 1000, id 1)
        HeapNode top1 = heap.extractTop();
        TEST_ASSERT_EQ(top1.docId, 2);

        HeapNode top2 = heap.extractTop();
        TEST_ASSERT_EQ(top2.docId, 3);

        HeapNode top3 = heap.extractTop();
        TEST_ASSERT_EQ(top3.docId, 4);

        HeapNode top4 = heap.extractTop();
        TEST_ASSERT_EQ(top4.docId, 1);
    }
}

} // namespace sse::test
