#include "TestHarness.hpp"
#include "dsa/CustomDynamicArray.hpp"
#include <string>

namespace sse::test {

void runDynamicArrayTests() {
    std::cout << "[RUNNING] CustomDynamicArray Tests..." << std::endl;

    // Test 1: Construction & Initial State
    {
        CustomDynamicArray<int> arr;
        TEST_ASSERT(arr.empty());
        TEST_ASSERT_EQ(arr.size(), 0);
        TEST_ASSERT_EQ(arr.capacity(), 0);
    }

    // Test 2: Push Back & 1.5x Geometric Expansion
    {
        CustomDynamicArray<int> arr;
        for (int i = 0; i < 20; ++i) {
            arr.push_back(i * 10);
        }
        TEST_ASSERT_EQ(arr.size(), 20);
        TEST_ASSERT(arr.capacity() >= 20);
        for (int i = 0; i < 20; ++i) {
            TEST_ASSERT_EQ(arr[i], i * 10);
            TEST_ASSERT_EQ(arr.at(i), i * 10);
        }
    }

    // Test 3: Out of Bounds Access Throws
    {
        CustomDynamicArray<int> arr;
        arr.push_back(42);
        TEST_ASSERT_THROWS(arr.at(1), std::out_of_range);
        TEST_ASSERT_THROWS(arr.at(100), std::out_of_range);
    }

    // Test 4: Pop Back & Front/Back
    {
        CustomDynamicArray<std::string> arr;
        arr.push_back("Alpha");
        arr.push_back("Beta");
        arr.push_back("Gamma");

        TEST_ASSERT_EQ(arr.front(), std::string("Alpha"));
        TEST_ASSERT_EQ(arr.back(), std::string("Gamma"));

        arr.pop_back();
        TEST_ASSERT_EQ(arr.size(), 2);
        TEST_ASSERT_EQ(arr.back(), std::string("Beta"));
    }

    // Test 5: Deep Copy Semantics
    {
        CustomDynamicArray<std::string> orig;
        orig.push_back("hello");
        orig.push_back("world");

        CustomDynamicArray<std::string> clone(orig);
        TEST_ASSERT_EQ(clone.size(), 2);
        TEST_ASSERT_EQ(clone[0], std::string("hello"));

        clone[0] = "modified";
        TEST_ASSERT_EQ(orig[0], std::string("hello"));
        TEST_ASSERT_EQ(clone[0], std::string("modified"));

        CustomDynamicArray<std::string> assigned;
        assigned = orig;
        TEST_ASSERT_EQ(assigned.size(), 2);
        assigned.push_back("extra");
        TEST_ASSERT_EQ(orig.size(), 2);
        TEST_ASSERT_EQ(assigned.size(), 3);
    }

    // Test 6: Move Semantics
    {
        CustomDynamicArray<int> src;
        src.push_back(1);
        src.push_back(2);
        src.push_back(3);

        CustomDynamicArray<int> moved(std::move(src));
        TEST_ASSERT_EQ(moved.size(), 3);
        TEST_ASSERT_EQ(moved[0], 1);
        TEST_ASSERT_EQ(src.size(), 0);
        TEST_ASSERT_EQ(src.capacity(), 0);
        TEST_ASSERT(src.data() == nullptr);
    }

    // Test 7: Range-based for loop & iterators
    {
        CustomDynamicArray<int> arr;
        arr.push_back(10);
        arr.push_back(20);
        arr.push_back(30);

        int sum = 0;
        for (int v : arr) {
            sum += v;
        }
        TEST_ASSERT_EQ(sum, 60);
    }

    // Test 8: Reserve & Shrink To Fit
    {
        CustomDynamicArray<int> arr;
        arr.reserve(50);
        TEST_ASSERT_EQ(arr.capacity(), 50);
        TEST_ASSERT_EQ(arr.size(), 0);

        arr.push_back(5);
        arr.push_back(15);
        arr.shrink_to_fit();
        TEST_ASSERT_EQ(arr.capacity(), 2);
        TEST_ASSERT_EQ(arr.size(), 2);
    }
}

} // namespace sse::test
