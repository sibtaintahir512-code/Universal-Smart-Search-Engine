#include "TestHarness.hpp"
#include "dsa/CustomPostingList.hpp"

namespace sse::test {

void runPostingListTests() {
    std::cout << "[RUNNING] CustomPostingList Tests..." << std::endl;

    // Test 1: Empty state
    {
        CustomPostingList list;
        TEST_ASSERT(list.empty());
        TEST_ASSERT_EQ(list.size(), 0);
        TEST_ASSERT(list.head() == nullptr);
        TEST_ASSERT(list.tail() == nullptr);
    }

    // Test 2: Monotonic Append & Independent Field Frequencies
    {
        CustomPostingList list;
        // Document 1 occurrences
        list.addOccurrence(1, true);  // Filename match
        list.addOccurrence(1, false); // Body match
        list.addOccurrence(1, false); // Body match

        // Document 3 occurrences
        list.addOccurrence(3, false); // Body match

        TEST_ASSERT_EQ(list.size(), 2);
        TEST_ASSERT_EQ(list.documentFrequency(), 2);

        const PostingNode* n1 = list.head();
        TEST_ASSERT(n1 != nullptr);
        TEST_ASSERT_EQ(n1->docId, 1);
        TEST_ASSERT_EQ(n1->nameTermFreq, 1);
        TEST_ASSERT_EQ(n1->bodyTermFreq, 2);
        TEST_ASSERT_EQ(n1->fieldMask, FIELD_FILENAME | FIELD_CONTENT);

        const PostingNode* n2 = n1->next;
        TEST_ASSERT(n2 != nullptr);
        TEST_ASSERT_EQ(n2->docId, 3);
        TEST_ASSERT_EQ(n2->nameTermFreq, 0);
        TEST_ASSERT_EQ(n2->bodyTermFreq, 1);
        TEST_ASSERT_EQ(n2->fieldMask, FIELD_CONTENT);
        TEST_ASSERT(n2->next == nullptr);
    }

    // Test 3: Out-of-order Sorted Fallback
    {
        CustomPostingList list;
        list.appendNode(10, 1, 0, FIELD_FILENAME);
        list.appendNode(30, 0, 2, FIELD_CONTENT);
        list.appendNode(20, 2, 1, FIELD_FILENAME | FIELD_CONTENT); // Out of order insert

        TEST_ASSERT_EQ(list.size(), 3);
        const PostingNode* curr = list.head();
        TEST_ASSERT_EQ(curr->docId, 10);
        curr = curr->next;
        TEST_ASSERT_EQ(curr->docId, 20);
        curr = curr->next;
        TEST_ASSERT_EQ(curr->docId, 30);
        TEST_ASSERT(curr->next == nullptr);
    }

    // Test 4: Set Intersection (AND)
    {
        // List A: Docs 1, 3, 5, 7
        CustomPostingList listA;
        listA.appendNode(1, 1, 0, FIELD_FILENAME);
        listA.appendNode(3, 0, 1, FIELD_CONTENT);
        listA.appendNode(5, 2, 2, FIELD_FILENAME | FIELD_CONTENT);
        listA.appendNode(7, 0, 1, FIELD_CONTENT);

        // List B: Docs 2, 3, 5, 8
        CustomPostingList listB;
        listB.appendNode(2, 1, 0, FIELD_FILENAME);
        listB.appendNode(3, 1, 1, FIELD_FILENAME | FIELD_CONTENT);
        listB.appendNode(5, 0, 3, FIELD_CONTENT);
        listB.appendNode(8, 0, 1, FIELD_CONTENT);

        CustomPostingList andResult = CustomPostingList::intersect(&listA, &listB);
        // Intersect should be Docs 3, 5
        TEST_ASSERT_EQ(andResult.size(), 2);

        const PostingNode* r1 = andResult.head();
        TEST_ASSERT(r1 != nullptr);
        TEST_ASSERT_EQ(r1->docId, 3);
        TEST_ASSERT_EQ(r1->nameTermFreq, 1); // 0 + 1
        TEST_ASSERT_EQ(r1->bodyTermFreq, 2); // 1 + 1

        const PostingNode* r2 = r1->next;
        TEST_ASSERT(r2 != nullptr);
        TEST_ASSERT_EQ(r2->docId, 5);
        TEST_ASSERT_EQ(r2->nameTermFreq, 2); // 2 + 0
        TEST_ASSERT_EQ(r2->bodyTermFreq, 5); // 2 + 3
    }

    // Test 5: Set Union (OR)
    {
        // List A: Docs 1, 4
        CustomPostingList listA;
        listA.appendNode(1, 1, 0, FIELD_FILENAME);
        listA.appendNode(4, 0, 2, FIELD_CONTENT);

        // List B: Docs 2, 4, 6
        CustomPostingList listB;
        listB.appendNode(2, 0, 1, FIELD_CONTENT);
        listB.appendNode(4, 1, 1, FIELD_FILENAME | FIELD_CONTENT);
        listB.appendNode(6, 1, 0, FIELD_FILENAME);

        CustomPostingList orResult = CustomPostingList::unionLists(&listA, &listB);
        // Union should be Docs 1, 2, 4, 6
        TEST_ASSERT_EQ(orResult.size(), 4);

        const PostingNode* c = orResult.head();
        TEST_ASSERT_EQ(c->docId, 1);
        c = c->next;
        TEST_ASSERT_EQ(c->docId, 2);
        c = c->next;
        TEST_ASSERT_EQ(c->docId, 4);
        TEST_ASSERT_EQ(c->nameTermFreq, 1); // 0 + 1
        TEST_ASSERT_EQ(c->bodyTermFreq, 3); // 2 + 1
        c = c->next;
        TEST_ASSERT_EQ(c->docId, 6);
        TEST_ASSERT(c->next == nullptr);
    }

    // Test 6: Set Difference (NOT)
    {
        // Universe: Docs 1, 2, 3, 4, 5
        CustomPostingList universe;
        for (DocID id = 1; id <= 5; ++id) {
            universe.appendNode(id, 1, 0, FIELD_FILENAME);
        }

        // Exclude: Docs 2, 4
        CustomPostingList exclude;
        exclude.appendNode(2, 1, 0, FIELD_FILENAME);
        exclude.appendNode(4, 1, 0, FIELD_FILENAME);

        CustomPostingList notResult = CustomPostingList::difference(&universe, &exclude);
        // Result should be Docs 1, 3, 5
        TEST_ASSERT_EQ(notResult.size(), 3);

        const PostingNode* c = notResult.head();
        TEST_ASSERT_EQ(c->docId, 1);
        c = c->next;
        TEST_ASSERT_EQ(c->docId, 3);
        c = c->next;
        TEST_ASSERT_EQ(c->docId, 5);
        TEST_ASSERT(c->next == nullptr);
    }

    // Test 7: Copy & Move Semantics
    {
        CustomPostingList orig;
        orig.appendNode(10, 1, 1, FIELD_FILENAME);
        orig.appendNode(20, 2, 2, FIELD_CONTENT);

        CustomPostingList copy(orig);
        TEST_ASSERT_EQ(copy.size(), 2);
        copy.appendNode(30, 3, 3, FIELD_CONTENT);
        TEST_ASSERT_EQ(orig.size(), 2);
        TEST_ASSERT_EQ(copy.size(), 3);

        CustomPostingList moved(std::move(orig));
        TEST_ASSERT_EQ(moved.size(), 2);
        TEST_ASSERT(orig.empty());
    }
}

} // namespace sse::test
