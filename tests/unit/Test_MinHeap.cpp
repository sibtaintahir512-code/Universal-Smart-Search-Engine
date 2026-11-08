#include "TestHarness.hpp"
#include "dsa/CustomMinHeap.hpp"
#include "core/CommonTypes.hpp"

namespace sse::test {

void runMinHeapTests() {
    std::cout << "[RUNNING] CustomMinHeap Tests..." << std::endl;

    // Test 1: Empty state & underflow
    {
        CustomMinHeap<int> heap;
        TEST_ASSERT(heap.empty());
        TEST_ASSERT_EQ(heap.size(), 0);
        TEST_ASSERT_THROWS(heap.top(), std::underflow_error);
    }

    // Test 2: Push & Ascending Order Extraction
    {
        CustomMinHeap<int> heap;
        heap.push(30);
        heap.push(10);
        heap.push(50);
        heap.push(20);

        TEST_ASSERT_EQ(heap.size(), 4);
        TEST_ASSERT_EQ(heap.top(), 10);

        TEST_ASSERT_EQ(heap.extractTop(), 10);
        TEST_ASSERT_EQ(heap.extractTop(), 20);
        TEST_ASSERT_EQ(heap.extractTop(), 30);
        TEST_ASSERT_EQ(heap.extractTop(), 50);
        TEST_ASSERT(heap.empty());
    }

    // Test 3: Floyd's Linear Build Heap
    {
        CustomDynamicArray<int> data;
        data.push_back(40);
        data.push_back(10);
        data.push_back(30);
        data.push_back(5);

        CustomMinHeap<int> heap(std::move(data));
        TEST_ASSERT_EQ(heap.extractMin(), 5);
        TEST_ASSERT_EQ(heap.extractMin(), 10);
        TEST_ASSERT_EQ(heap.extractMin(), 30);
        TEST_ASSERT_EQ(heap.extractMin(), 40);
    }

    // Test 4: Strategy B Verification (Bounded Min-Heap of Size K = 3)
    // Stream 8 candidates, keep only Top-3 highest scoring documents
    {
        constexpr std::size_t K = 3;
        CustomMinHeap<HeapNode> boundedHeap;

        // Candidate stream with varying scores
        HeapNode stream[] = {
            HeapNode{1, 2.5f, 0},
            HeapNode{2, 8.0f, 0},
            HeapNode{3, 1.0f, 0},
            HeapNode{4, 9.5f, 0},
            HeapNode{5, 7.2f, 0},
            HeapNode{6, 3.1f, 0},
            HeapNode{7, 4.0f, 0},
            HeapNode{8, 6.8f, 0}
        };

        for (const auto& candidate : stream) {
            if (boundedHeap.size() < K) {
                boundedHeap.push(candidate);
            } else if (candidate > boundedHeap.top()) { // candidate has higher score than current K-th minimum
                boundedHeap.replaceTop(candidate);
            }
        }

        TEST_ASSERT_EQ(boundedHeap.size(), K);

        // Top 3 scores in the stream are: 9.5 (Doc 4), 8.0 (Doc 2), 7.2 (Doc 5)
        // In the min-heap, root should be the 3rd highest (Doc 5, score 7.2)
        TEST_ASSERT_EQ(boundedHeap.top().docId, 5);

        HeapNode r1 = boundedHeap.extractTop(); // 7.2
        HeapNode r2 = boundedHeap.extractTop(); // 8.0
        HeapNode r3 = boundedHeap.extractTop(); // 9.5

        TEST_ASSERT_EQ(r1.docId, 5);
        TEST_ASSERT_EQ(r2.docId, 2);
        TEST_ASSERT_EQ(r3.docId, 4);
    }
}

} // namespace sse::test
