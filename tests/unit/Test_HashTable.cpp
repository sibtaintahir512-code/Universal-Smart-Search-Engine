#include "TestHarness.hpp"
#include "dsa/CustomHashTable.hpp"
#include <string>

namespace sse::test {

void runHashTableTests() {
    std::cout << "[RUNNING] CustomHashTable Tests..." << std::endl;

    // Test 1: Construction & Initial State
    {
        CustomHashTable<std::string, int> table;
        TEST_ASSERT(table.empty());
        TEST_ASSERT_EQ(table.size(), 0);
        TEST_ASSERT_EQ(table.bucketCount(), 16); // Power of two initial capacity
    }

    // Test 2: Insert & Lookup
    {
        CustomHashTable<std::string, int> table;
        table.insert("apple", 100);
        table.insert("banana", 200);
        table.insert("cherry", 300);

        TEST_ASSERT_EQ(table.size(), 3);
        TEST_ASSERT(table.contains("apple"));
        TEST_ASSERT(table.contains("banana"));
        TEST_ASSERT(table.contains("cherry"));
        TEST_ASSERT(!table.contains("durian"));

        int* val = table.find("banana");
        TEST_ASSERT(val != nullptr);
        TEST_ASSERT_EQ(*val, 200);
    }

    // Test 3: Operator[] & Value Mutation
    {
        CustomHashTable<std::string, int> table;
        table["counter"] = 1;
        table["counter"] += 5;

        TEST_ASSERT_EQ(table["counter"], 6);
        TEST_ASSERT_EQ(table.size(), 1);
    }

    // Test 4: Erase Operation
    {
        CustomHashTable<std::string, int> table;
        table.insert("k1", 1);
        table.insert("k2", 2);
        table.insert("k3", 3);

        TEST_ASSERT(table.erase("k2"));
        TEST_ASSERT_EQ(table.size(), 2);
        TEST_ASSERT(!table.contains("k2"));
        TEST_ASSERT(!table.erase("k2")); // Double erase returns false
    }

    // Test 5: Dynamic Rehashing (> 0.75 Load Factor)
    {
        CustomHashTable<std::string, int> table(8); // Capacity 8, rehashes at > 6
        TEST_ASSERT_EQ(table.bucketCount(), 8);

        for (int i = 0; i < 7; ++i) {
            table.insert("key" + std::to_string(i), i * 10);
        }

        TEST_ASSERT(table.bucketCount() > 8); // Capacity doubled to 16
        for (int i = 0; i < 7; ++i) {
            int* v = table.find("key" + std::to_string(i));
            TEST_ASSERT(v != nullptr);
            TEST_ASSERT_EQ(*v, i * 10);
        }
    }

    // Test 6: Multilingual UTF-8 Key Integrity (Zero Information Loss invariant)
    {
        CustomHashTable<std::string, std::string> table;
        table.insert("دستاویز", "Urdu Document");
        table.insert("سبق", "Urdu Lesson");
        table.insert("café", "French Coffee");
        table.insert("北京", "Chinese Beijing");

        TEST_ASSERT_EQ(table.size(), 4);
        TEST_ASSERT(table.contains("دستاویز"));
        TEST_ASSERT(table.contains("سبق"));
        TEST_ASSERT(table.contains("café"));
        TEST_ASSERT(table.contains("北京"));

        std::string* uVal = table.find("دستاویز");
        TEST_ASSERT(uVal != nullptr);
        TEST_ASSERT_EQ(*uVal, std::string("Urdu Document"));
    }

    // Test 7: Deep Copy & Move Semantics
    {
        CustomHashTable<std::string, int> orig;
        orig.insert("one", 1);
        orig.insert("two", 2);

        CustomHashTable<std::string, int> copy(orig);
        TEST_ASSERT_EQ(copy.size(), 2);
        copy.insert("three", 3);
        TEST_ASSERT_EQ(orig.size(), 2);
        TEST_ASSERT_EQ(copy.size(), 3);

        CustomHashTable<std::string, int> moved(std::move(orig));
        TEST_ASSERT_EQ(moved.size(), 2);
        TEST_ASSERT(orig.empty());
    }

    // Test 8: Bulk Insertion Stress Test (1,000 keys)
    {
        CustomHashTable<std::string, int> table;
        for (int i = 0; i < 1000; ++i) {
            table.insert("term_" + std::to_string(i), i);
        }
        TEST_ASSERT_EQ(table.size(), 1000);
        for (int i = 0; i < 1000; ++i) {
            TEST_ASSERT(table.contains("term_" + std::to_string(i)));
        }
    }
}

} // namespace sse::test
