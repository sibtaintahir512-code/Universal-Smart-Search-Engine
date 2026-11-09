#include "TestHarness.hpp"
#include "indexing/InvertedIndex.hpp"

namespace sse::test {

void runInvertedIndexTests() {
    std::cout << "[RUNNING] InvertedIndex Tests..." << std::endl;

    using namespace sse::indexing;

    // Test 1: Empty Index Properties
    {
        InvertedIndex index;
        TEST_ASSERT_EQ(index.vocabularySize(), 0ULL);
        TEST_ASSERT_EQ(index.totalPostingsCount(), 0ULL);
        TEST_ASSERT_EQ(index.trieVocabularySize(), 0ULL);
        TEST_ASSERT(!index.containsTerm("search"));
        TEST_ASSERT(index.getPostingList("search") == nullptr);
        TEST_ASSERT_EQ(index.getDocumentFrequency("search"), 0U);
    }

    // Test 2: Insert Single Term and Inspect PostingNode
    {
        InvertedIndex index;
        index.addTerm("engine", 1, 1, 0, FIELD_FILENAME);

        TEST_ASSERT_EQ(index.vocabularySize(), 1ULL);
        TEST_ASSERT_EQ(index.totalPostingsCount(), 1ULL);
        TEST_ASSERT(index.containsTerm("engine"));

        const dsa::CustomPostingList* plist = index.getPostingList("engine");
        TEST_ASSERT(plist != nullptr);
        TEST_ASSERT_EQ(plist->documentFrequency(), 1U);
        TEST_ASSERT_EQ(index.getDocumentFrequency("engine"), 1U);

        const PostingNode* head = plist->head();
        TEST_ASSERT(head != nullptr);
        TEST_ASSERT_EQ(head->docId, 1U);
        TEST_ASSERT_EQ(head->nameTermFreq, 1U);
        TEST_ASSERT_EQ(head->bodyTermFreq, 0U);
        TEST_ASSERT_EQ(head->fieldMask, FIELD_FILENAME);
    }

    // Test 3: Multiple Documents Containing the Same Term (Specification Invariant: DF = distinct docs)
    {
        InvertedIndex index;
        // Document 1 contains "search" in filename
        index.addTerm("search", 1, 2, 0, FIELD_FILENAME);
        // Document 2 contains "search" in body
        index.addTerm("search", 2, 0, 5, FIELD_CONTENT);
        // Document 3 contains "search" in both filename and body
        index.addTerm("search", 3, 1, 3, FIELD_FILENAME | FIELD_CONTENT);

        TEST_ASSERT_EQ(index.vocabularySize(), 1ULL);
        TEST_ASSERT_EQ(index.totalPostingsCount(), 3ULL);
        TEST_ASSERT_EQ(index.getDocumentFrequency("search"), 3U);

        const dsa::CustomPostingList* plist = index.getPostingList("search");
        TEST_ASSERT(plist != nullptr);

        // Verify sorted order (1 -> 2 -> 3)
        const PostingNode* n1 = plist->head();
        TEST_ASSERT(n1 != nullptr);
        TEST_ASSERT_EQ(n1->docId, 1U);
        TEST_ASSERT_EQ(n1->nameTermFreq, 2U);
        TEST_ASSERT_EQ(n1->bodyTermFreq, 0U);

        const PostingNode* n2 = n1->next;
        TEST_ASSERT(n2 != nullptr);
        TEST_ASSERT_EQ(n2->docId, 2U);
        TEST_ASSERT_EQ(n2->nameTermFreq, 0U);
        TEST_ASSERT_EQ(n2->bodyTermFreq, 5U);

        const PostingNode* n3 = n2->next;
        TEST_ASSERT(n3 != nullptr);
        TEST_ASSERT_EQ(n3->docId, 3U);
        TEST_ASSERT_EQ(n3->nameTermFreq, 1U);
        TEST_ASSERT_EQ(n3->bodyTermFreq, 3U);
        TEST_ASSERT_EQ(n3->fieldMask, FIELD_FILENAME | FIELD_CONTENT);
        TEST_ASSERT(n3->next == nullptr);
    }

    // Test 4: Merging Occurrences for the Same Document (No Duplicate Nodes)
    {
        InvertedIndex index;
        // Add "data" to doc 10 in filename
        index.addTerm("data", 10, 1, 0, FIELD_FILENAME);
        // Later add "data" to doc 10 in body
        index.addTerm("data", 10, 0, 4, FIELD_CONTENT);

        TEST_ASSERT_EQ(index.getDocumentFrequency("data"), 1U);
        TEST_ASSERT_EQ(index.totalPostingsCount(), 1ULL);

        const dsa::CustomPostingList* plist = index.getPostingList("data");
        TEST_ASSERT(plist != nullptr);
        const PostingNode* node = plist->head();
        TEST_ASSERT(node != nullptr);
        TEST_ASSERT_EQ(node->docId, 10U);
        TEST_ASSERT_EQ(node->nameTermFreq, 1U);
        TEST_ASSERT_EQ(node->bodyTermFreq, 4U);
        TEST_ASSERT_EQ(node->fieldMask, FIELD_FILENAME | FIELD_CONTENT);
        TEST_ASSERT(node->next == nullptr);
    }

    // Test 5: CustomTrie Synchronization (Latin [a-z0-9] vs Non-ASCII Bypass)
    {
        InvertedIndex index;
        index.addTerm("algorithm", 1, 1, 0, FIELD_FILENAME);
        index.addTerm("cs101", 1, 0, 1, FIELD_CONTENT);
        // Non-ASCII Urdu term "اردو"
        std::string urduTerm = "\xD8\xA7\xD8\xB1\xD8\xAF\xD9\x88";
        index.addTerm(urduTerm, 1, 1, 0, FIELD_FILENAME);

        TEST_ASSERT_EQ(index.vocabularySize(), 3ULL);
        // "algorithm" and "cs101" are eligible for CustomTrie
        TEST_ASSERT(index.trie().contains("algorithm"));
        TEST_ASSERT(index.trie().contains("cs101"));
        TEST_ASSERT_EQ(index.trieVocabularySize(), 2ULL);

        // Urdu term is 100% indexed in CustomHashTable but bypassed Trie safely
        TEST_ASSERT(index.containsTerm(urduTerm));
        TEST_ASSERT(!index.trie().contains(urduTerm));
    }

    // Test 6: Document Removal and Empty Posting Cleanup
    {
        InvertedIndex index;
        // Doc 1 has "alpha", "beta"
        index.addTerm("alpha", 1, 1, 0, FIELD_FILENAME);
        index.addTerm("beta", 1, 1, 0, FIELD_FILENAME);
        // Doc 2 has "beta", "gamma"
        index.addTerm("beta", 2, 1, 0, FIELD_FILENAME);
        index.addTerm("gamma", 2, 1, 0, FIELD_FILENAME);

        TEST_ASSERT_EQ(index.vocabularySize(), 3ULL);
        TEST_ASSERT_EQ(index.totalPostingsCount(), 4ULL);

        // Remove Doc 1
        bool removed = index.removeDocument(1);
        TEST_ASSERT(removed);

        // "alpha" was only in Doc 1 -> posting list became empty -> term purged completely
        TEST_ASSERT(!index.containsTerm("alpha"));
        TEST_ASSERT(index.getPostingList("alpha") == nullptr);
        TEST_ASSERT(!index.trie().contains("alpha"));

        // "beta" remains because Doc 2 contains it
        TEST_ASSERT(index.containsTerm("beta"));
        TEST_ASSERT_EQ(index.getDocumentFrequency("beta"), 1U);
        TEST_ASSERT(index.trie().contains("beta"));

        // "gamma" remains
        TEST_ASSERT(index.containsTerm("gamma"));
        TEST_ASSERT_EQ(index.getDocumentFrequency("gamma"), 1U);

        TEST_ASSERT_EQ(index.vocabularySize(), 2ULL);
        TEST_ASSERT_EQ(index.totalPostingsCount(), 2ULL);

        // Remove non-existent doc
        TEST_ASSERT(!index.removeDocument(999));
    }

    // Test 7: Explicit Term Removal and Clear
    {
        InvertedIndex index;
        index.addTerm("one", 1, 1, 0, FIELD_FILENAME);
        index.addTerm("two", 1, 1, 0, FIELD_FILENAME);
        index.addTerm("three", 2, 1, 0, FIELD_FILENAME);

        TEST_ASSERT_EQ(index.vocabularySize(), 3ULL);
        TEST_ASSERT(index.removeTerm("two"));
        TEST_ASSERT(!index.containsTerm("two"));
        TEST_ASSERT(!index.trie().contains("two"));
        TEST_ASSERT_EQ(index.vocabularySize(), 2ULL);

        index.clear();
        TEST_ASSERT_EQ(index.vocabularySize(), 0ULL);
        TEST_ASSERT_EQ(index.totalPostingsCount(), 0ULL);
        TEST_ASSERT_EQ(index.trieVocabularySize(), 0ULL);
    }
}

} // namespace sse::test
