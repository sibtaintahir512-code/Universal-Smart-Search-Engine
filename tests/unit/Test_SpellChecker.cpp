#include "TestHarness.hpp"
#include "search/SpellChecker.hpp"
#include "dsa/CustomTrie.hpp"

namespace sse::test {

void runSpellCheckerTests() {
    std::cout << "[RUNNING] SpellChecker Tests (AC-10 Verification)..." << std::endl;

    // Test 1: Algorithmic Levenshtein Distance
    {
        TEST_ASSERT_EQ(search::SpellChecker::computeLevenshteinDistance("", ""), 0);
        TEST_ASSERT_EQ(search::SpellChecker::computeLevenshteinDistance("kitten", "sitting"), 3);
        TEST_ASSERT_EQ(search::SpellChecker::computeLevenshteinDistance("flaw", "lawn"), 2);
        TEST_ASSERT_EQ(search::SpellChecker::computeLevenshteinDistance("algorithm", "algorithm"), 0);
        TEST_ASSERT_EQ(search::SpellChecker::computeLevenshteinDistance("algorithm", "algorthm"), 1); // 1 deletion
        TEST_ASSERT_EQ(search::SpellChecker::computeLevenshteinDistance("algorithm", "algorithms"), 1); // 1 insertion
        TEST_ASSERT_EQ(search::SpellChecker::computeLevenshteinDistance("algorithm", "algoritbm"), 1); // 1 substitution
        TEST_ASSERT_EQ(search::SpellChecker::computeLevenshteinDistance("algorithm", "algortbm"), 2); // 1 del + 1 sub
        TEST_ASSERT_EQ(search::SpellChecker::computeLevenshteinDistance("algorithm", "alzzrtbm"), 4); // > 2 edits
    }

    // Test 2: Trie Vocabulary Setup & Exact Match
    {
        dsa::CustomTrie trie;
        trie.insert("algorithm", 100);
        trie.insert("algebra", 50);
        trie.insert("dijkstra", 80);
        trie.insert("binary", 60);

        search::SpellChecker checker(&trie);
        auto res = checker.suggestDetailed("algorithm", 2, 5);
        TEST_ASSERT(!res.empty());
        TEST_ASSERT_EQ(res[0].term, "algorithm");
        TEST_ASSERT_EQ(res[0].distance, 0);
        TEST_ASSERT_EQ(res[0].frequency, 100);
    }

    // Test 3: 1-Deletion Correction ("algorthm" -> "algorithm")
    {
        dsa::CustomTrie trie;
        trie.insert("algorithm", 100);
        trie.insert("algebra", 50);

        search::SpellChecker checker(&trie);
        auto res = checker.suggestDetailed("algorthm", 2, 5);
        TEST_ASSERT(!res.empty());
        TEST_ASSERT_EQ(res[0].term, "algorithm");
        TEST_ASSERT_EQ(res[0].distance, 1);
    }

    // Test 4: 1-Insertion Correction ("algorithms" -> "algorithm")
    {
        dsa::CustomTrie trie;
        trie.insert("algorithm", 100);

        search::SpellChecker checker(&trie);
        auto res = checker.suggestDetailed("algorithms", 2, 5);
        TEST_ASSERT(!res.empty());
        TEST_ASSERT_EQ(res[0].term, "algorithm");
        TEST_ASSERT_EQ(res[0].distance, 1);
    }

    // Test 5: 1-Substitution Correction ("algoritbm" -> "algorithm")
    {
        dsa::CustomTrie trie;
        trie.insert("algorithm", 100);

        search::SpellChecker checker(&trie);
        auto res = checker.suggestDetailed("algoritbm", 2, 5);
        TEST_ASSERT(!res.empty());
        TEST_ASSERT_EQ(res[0].term, "algorithm");
        TEST_ASSERT_EQ(res[0].distance, 1);
    }

    // Test 6: 2-Edits Correction ("algortbm" -> "algorithm")
    {
        dsa::CustomTrie trie;
        trie.insert("algorithm", 100);
        trie.insert("dijkstra", 50);

        search::SpellChecker checker(&trie);
        auto res = checker.suggestDetailed("algortbm", 2, 5);
        TEST_ASSERT(!res.empty());
        TEST_ASSERT_EQ(res[0].term, "algorithm");
        TEST_ASSERT_EQ(res[0].distance, 2);
    }

    // Test 7: 3-Edits Rejection (Distance > 2 must not be returned)
    {
        dsa::CustomTrie trie;
        trie.insert("algorithm", 100);
        trie.insert("dijkstra", 50);

        search::SpellChecker checker(&trie);
        // "alzzrtbm" has edit distance 4 from algorithm
        auto res = checker.suggestDetailed("alzzrtbm", 2, 5);
        TEST_ASSERT(res.empty());

        auto res2 = checker.suggest("completelydifferentword", 2, 5);
        TEST_ASSERT(res2.empty());
    }

    // Test 8: Empty and Short Query Safety
    {
        dsa::CustomTrie trie;
        trie.insert("search", 10);

        search::SpellChecker checker(&trie);
        auto resEmpty = checker.suggest("", 2, 5);
        TEST_ASSERT(resEmpty.empty());

        search::SpellChecker checkerNull(nullptr);
        auto resNull = checkerNull.suggest("search", 2, 5);
        TEST_ASSERT(resNull.empty());
    }

    // Test 9: Case-folding and Special Characters Normalization
    {
        dsa::CustomTrie trie;
        trie.insert("dijkstra", 75);

        search::SpellChecker checker(&trie);
        auto resUpper = checker.suggest("DIJKSTRA", 2, 5);
        TEST_ASSERT(!resUpper.empty());
        TEST_ASSERT_EQ(resUpper[0], "dijkstra");

        auto resMixed = checker.suggest("DijkStr", 2, 5);
        TEST_ASSERT(!resMixed.empty());
        TEST_ASSERT_EQ(resMixed[0], "dijkstra");
    }

    // Test 10: Multi-Candidate Ranking (Distance first, frequency tie-breaker)
    {
        dsa::CustomTrie trie;
        trie.insert("test", 10);
        trie.insert("best", 100); // 1 edit from rest
        trie.insert("rest", 50);  // 0 edit from rest
        trie.insert("nest", 20);  // 1 edit from rest

        search::SpellChecker checker(&trie);
        auto res = checker.suggestDetailed("rest", 2, 5);
        TEST_ASSERT(res.size() >= 3);
        // Exact match distance 0 must be first
        TEST_ASSERT_EQ(res[0].term, "rest");
        TEST_ASSERT_EQ(res[0].distance, 0);

        // Distance 1 candidates: "best" (freq 100) vs "nest" (freq 20) vs "test" (freq 10)
        // best has higher frequency so should rank ahead of nest and test
        TEST_ASSERT_EQ(res[1].term, "best");
        TEST_ASSERT_EQ(res[1].distance, 1);
        TEST_ASSERT_EQ(res[1].frequency, 100);
    }
}

} // namespace sse::test
