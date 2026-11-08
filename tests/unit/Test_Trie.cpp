#include "TestHarness.hpp"
#include "dsa/CustomTrie.hpp"

namespace sse::test {

void runTrieTests() {
    std::cout << "[RUNNING] CustomTrie Tests..." << std::endl;

    // Test 1: Empty state
    {
        CustomTrie trie;
        TEST_ASSERT(trie.empty());
        TEST_ASSERT_EQ(trie.wordCount(), 0);
        TEST_ASSERT_EQ(trie.nodeCount(), 0);
    }

    // Test 2: Valid Insertions & Lowercase Normalization
    {
        CustomTrie trie;
        TEST_ASSERT(trie.insert("Algorithm", 5));
        TEST_ASSERT(trie.insert("algebra", 3));
        TEST_ASSERT(trie.insert("cplusplus20", 10));

        TEST_ASSERT_EQ(trie.wordCount(), 3);
        TEST_ASSERT(trie.contains("algorithm")); // Normalized from "Algorithm"
        TEST_ASSERT(trie.contains("algebra"));
        TEST_ASSERT(trie.contains("cplusplus20"));
        TEST_ASSERT(!trie.contains("alg"));      // Prefix, not whole word

        TEST_ASSERT_EQ(trie.getFrequency("algorithm"), 5);
        TEST_ASSERT_EQ(trie.getFrequency("cplusplus20"), 10);
        TEST_ASSERT_EQ(trie.getFrequency("nonexistent"), 0);
    }

    // Test 3: Frequency Accumulation
    {
        CustomTrie trie;
        trie.insert("search", 2);
        trie.insert("search", 7);
        TEST_ASSERT_EQ(trie.getFrequency("search"), 9);
        TEST_ASSERT_EQ(trie.wordCount(), 1);
    }

    // Test 4: Rejection of Invalid Non-Alphanumeric Words
    {
        CustomTrie trie;
        TEST_ASSERT(!trie.insert("hello world")); // Space not allowed in Trie token
        TEST_ASSERT(!trie.insert("c++"));         // Plus not in [a-z0-9]
        TEST_ASSERT(!trie.insert(""));            // Empty string rejected
        TEST_ASSERT(!trie.insert("دستاویز"));     // Non-ASCII rejected by Trie ingestion filter
        TEST_ASSERT_EQ(trie.wordCount(), 0);
    }

    // Test 5: Prefix Check & Prefix Node
    {
        CustomTrie trie;
        trie.insert("datastructure");
        trie.insert("database");

        TEST_ASSERT(trie.hasPrefix("data"));
        TEST_ASSERT(trie.hasPrefix("datas"));
        TEST_ASSERT(!trie.hasPrefix("date"));

        const TrieNode* pNode = trie.findPrefixNode("data");
        TEST_ASSERT(pNode != nullptr);
        TEST_ASSERT(!pNode->isEndOfWord); // "data" itself was not inserted as terminal
    }

    // Test 6: Top-5 Autocomplete Suggestions Ordered by Frequency
    {
        CustomTrie trie;
        trie.insert("apple", 10);
        trie.insert("application", 50);
        trie.insert("apply", 30);
        trie.insert("app", 5);
        trie.insert("applet", 25);
        trie.insert("appendix", 15);
        trie.insert("banana", 100);

        auto suggestions = trie.getTopKSuggestions("app", 5);
        TEST_ASSERT_EQ(suggestions.size(), 5);

        // Expected order:
        // 1. application (50)
        // 2. apply (30)
        // 3. applet (25)
        // 4. appendix (15)
        // 5. apple (10)
        TEST_ASSERT_EQ(suggestions[0].term, std::string("application"));
        TEST_ASSERT_EQ(suggestions[0].frequency, 50);

        TEST_ASSERT_EQ(suggestions[1].term, std::string("apply"));
        TEST_ASSERT_EQ(suggestions[1].frequency, 30);

        TEST_ASSERT_EQ(suggestions[2].term, std::string("applet"));
        TEST_ASSERT_EQ(suggestions[2].frequency, 25);

        TEST_ASSERT_EQ(suggestions[3].term, std::string("appendix"));
        TEST_ASSERT_EQ(suggestions[3].frequency, 15);

        TEST_ASSERT_EQ(suggestions[4].term, std::string("apple"));
        TEST_ASSERT_EQ(suggestions[4].frequency, 10);
    }

    // Test 7: Multilingual Query Prefix Fallback (Safe empty return)
    {
        CustomTrie trie;
        trie.insert("test", 1);
        auto suggestions = trie.getTopKSuggestions("اردو", 5);
        TEST_ASSERT(suggestions.empty()); // Safely returns empty without crashing
    }

    // Test 8: Copy and Move Semantics
    {
        CustomTrie orig;
        orig.insert("one", 1);
        orig.insert("two", 2);

        CustomTrie copy(orig);
        TEST_ASSERT_EQ(copy.wordCount(), 2);
        TEST_ASSERT(copy.contains("one"));
        copy.insert("three", 3);
        TEST_ASSERT_EQ(orig.wordCount(), 2);
        TEST_ASSERT_EQ(copy.wordCount(), 3);

        CustomTrie moved(std::move(orig));
        TEST_ASSERT_EQ(moved.wordCount(), 2);
        TEST_ASSERT(orig.empty());
    }
}

} // namespace sse::test
