#include "TestHarness.hpp"
#include "dsa/CustomQueue.hpp"
#include "dsa/CustomDynamicArray.hpp"
#include <string>

namespace sse::test {

void runQueueTests() {
    std::cout << "[RUNNING] CustomQueue Tests..." << std::endl;

    // Test 1: Empty state & underflow
    {
        CustomQueue<int> q;
        TEST_ASSERT(q.empty());
        TEST_ASSERT_EQ(q.size(), 0);
        TEST_ASSERT_THROWS(q.front(), std::underflow_error);
        TEST_ASSERT_THROWS(q.back(), std::underflow_error);
        TEST_ASSERT_THROWS(q.dequeue(), std::underflow_error);
    }

    // Test 2: FIFO Invariant
    {
        CustomQueue<std::string> q;
        q.enqueue("First");
        q.enqueue("Second");
        q.enqueue("Third");

        TEST_ASSERT_EQ(q.size(), 3);
        TEST_ASSERT_EQ(q.front(), std::string("First"));
        TEST_ASSERT_EQ(q.back(), std::string("Third"));

        TEST_ASSERT_EQ(q.popFront(), std::string("First"));
        TEST_ASSERT_EQ(q.size(), 2);
        TEST_ASSERT_EQ(q.front(), std::string("Second"));

        TEST_ASSERT_EQ(q.popFront(), std::string("Second"));
        TEST_ASSERT_EQ(q.front(), std::string("Third"));
        TEST_ASSERT_EQ(q.back(), std::string("Third"));

        TEST_ASSERT_EQ(q.popFront(), std::string("Third"));
        TEST_ASSERT(q.empty());
    }

    // Test 3: Deep Copy & Move Semantics
    {
        CustomQueue<int> orig;
        orig.enqueue(10);
        orig.enqueue(20);

        CustomQueue<int> copy(orig);
        TEST_ASSERT_EQ(copy.size(), 2);
        TEST_ASSERT_EQ(copy.front(), 10);
        copy.enqueue(30);
        TEST_ASSERT_EQ(orig.size(), 2);
        TEST_ASSERT_EQ(copy.size(), 3);

        CustomQueue<int> moved(std::move(orig));
        TEST_ASSERT_EQ(moved.size(), 2);
        TEST_ASSERT(orig.empty());
    }

    // Test 4: BFS Directory Traversal Simulation
    {
        // Simulate enqueueing directory paths in Breadth-First order
        CustomQueue<std::string> dirQueue;
        dirQueue.enqueue("C:/Root");

        CustomDynamicArray<std::string> visited;

        // Level 1
        std::string curr = dirQueue.popFront();
        visited.push_back(curr);
        dirQueue.enqueue(curr + "/Docs");
        dirQueue.enqueue(curr + "/Images");
        dirQueue.enqueue(curr + "/Code");

        // Process Level 2
        while (!dirQueue.empty()) {
            visited.push_back(dirQueue.popFront());
        }

        TEST_ASSERT_EQ(visited.size(), 4);
        TEST_ASSERT_EQ(visited[0], std::string("C:/Root"));
        TEST_ASSERT_EQ(visited[1], std::string("C:/Root/Docs"));
        TEST_ASSERT_EQ(visited[2], std::string("C:/Root/Images"));
        TEST_ASSERT_EQ(visited[3], std::string("C:/Root/Code"));
    }
}

} // namespace sse::test
