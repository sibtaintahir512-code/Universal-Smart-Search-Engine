#include "TestHarness.hpp"
#include "dsa/CustomSinglyLinkedList.hpp"
#include <string>

namespace sse::test {

void runSinglyLinkedListTests() {
    std::cout << "[RUNNING] CustomSinglyLinkedList Tests..." << std::endl;

    // Test 1: Empty state
    {
        CustomSinglyLinkedList<int> list;
        TEST_ASSERT(list.empty());
        TEST_ASSERT_EQ(list.size(), 0);
    }

    // Test 2: Push back and front
    {
        CustomSinglyLinkedList<int> list;
        list.push_back(10);
        list.push_back(20);
        list.push_front(5);

        TEST_ASSERT_EQ(list.size(), 3);
        TEST_ASSERT_EQ(list.front(), 5);
        TEST_ASSERT_EQ(list.back(), 20);
    }

    // Test 3: Pop front
    {
        CustomSinglyLinkedList<int> list;
        list.push_back(1);
        list.push_back(2);
        list.push_back(3);

        list.pop_front();
        TEST_ASSERT_EQ(list.size(), 2);
        TEST_ASSERT_EQ(list.front(), 2);

        list.pop_front();
        TEST_ASSERT_EQ(list.front(), 3);
        TEST_ASSERT_EQ(list.back(), 3);

        list.pop_front();
        TEST_ASSERT(list.empty());
        TEST_ASSERT_EQ(list.size(), 0);
    }

    // Test 4: remove_if predicate
    {
        CustomSinglyLinkedList<int> list;
        for (int i = 1; i <= 6; ++i) {
            list.push_back(i); // 1, 2, 3, 4, 5, 6
        }

        // Remove all even numbers
        std::size_t removed = list.remove_if([](int v) { return v % 2 == 0; });
        TEST_ASSERT_EQ(removed, 3);
        TEST_ASSERT_EQ(list.size(), 3);

        int expected[] = {1, 3, 5};
        int idx = 0;
        for (int v : list) {
            TEST_ASSERT_EQ(v, expected[idx++]);
        }
    }

    // Test 5: Deep Copy Semantics
    {
        CustomSinglyLinkedList<std::string> orig;
        orig.push_back("node1");
        orig.push_back("node2");

        CustomSinglyLinkedList<std::string> copy(orig);
        TEST_ASSERT_EQ(copy.size(), 2);
        TEST_ASSERT_EQ(copy.front(), std::string("node1"));

        copy.push_back("node3");
        TEST_ASSERT_EQ(orig.size(), 2);
        TEST_ASSERT_EQ(copy.size(), 3);
    }

    // Test 6: Move Semantics
    {
        CustomSinglyLinkedList<int> src;
        src.push_back(100);
        src.push_back(200);

        CustomSinglyLinkedList<int> dest(std::move(src));
        TEST_ASSERT_EQ(dest.size(), 2);
        TEST_ASSERT_EQ(dest.front(), 100);
        TEST_ASSERT(src.empty());
    }

    // Test 7: Non-recursive deallocation stress test (5,000 nodes)
    {
        CustomSinglyLinkedList<int> bigList;
        for (int i = 0; i < 5000; ++i) {
            bigList.push_back(i);
        }
        TEST_ASSERT_EQ(bigList.size(), 5000);
        bigList.clear();
        TEST_ASSERT(bigList.empty());
    }
}

} // namespace sse::test
