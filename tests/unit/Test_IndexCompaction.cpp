#include "TestHarness.hpp"
#include "persistence/IndexCompactor.hpp"
#include "persistence/IndexPersistenceManager.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"

namespace sse::test {

void runIndexCompactionTests() {
    std::cout << "[RUNNING] IndexCompaction Tests..." << std::endl;

    using namespace sse::indexing;
    using namespace sse::persistence;

    DocumentCatalog catalog;
    InvertedIndex index;

    FileMetadata m{};
    DocID id1 = catalog.registerDocument(m, "/file1.txt", "file1.txt"); // Doc 1
    DocID id2 = catalog.registerDocument(m, "/file2.txt", "file2.txt"); // Doc 2 (to be deleted)
    DocID id3 = catalog.registerDocument(m, "/file3.txt", "file3.txt"); // Doc 3
    DocID id4 = catalog.registerDocument(m, "/file4.txt", "file4.txt"); // Doc 4 (to be deleted)
    DocID id5 = catalog.registerDocument(m, "/file5.txt", "file5.txt"); // Doc 5

    // Postings
    index.addTerm("common", id1, 1, 1, FIELD_CONTENT);
    index.addTerm("common", id2, 1, 1, FIELD_CONTENT);
    index.addTerm("common", id3, 1, 1, FIELD_CONTENT);
    index.addTerm("common", id4, 1, 1, FIELD_CONTENT);
    index.addTerm("common", id5, 1, 1, FIELD_CONTENT);

    index.addTerm("deleted_only", id2, 1, 0, FIELD_FILENAME);
    index.addTerm("deleted_only", id4, 1, 0, FIELD_FILENAME);

    index.addTerm("live_term", id3, 0, 5, FIELD_CONTENT);

    // Document lengths
    index.documentLengthStatistics().recordDocumentLength(id1, 1, 10);
    index.documentLengthStatistics().recordDocumentLength(id2, 1, 20);
    index.documentLengthStatistics().recordDocumentLength(id3, 1, 30);
    index.documentLengthStatistics().recordDocumentLength(id4, 1, 40);
    index.documentLengthStatistics().recordDocumentLength(id5, 1, 50);

    TEST_ASSERT_EQ(catalog.totalCount(), 5);
    TEST_ASSERT_EQ(catalog.activeCount(), 5);

    // Soft delete / tombstone documents 2 and 4
    catalog.markDeleted(id2);
    catalog.markDeleted(id4);

    TEST_ASSERT_EQ(catalog.totalCount(), 5);
    TEST_ASSERT_EQ(catalog.activeCount(), 3);
    TEST_ASSERT(!catalog.isActive(id2));
    TEST_ASSERT(!catalog.isActive(id4));

    // 1. Execute Compaction
    DocumentCatalog compCat;
    InvertedIndex compIdx;

    PersistenceResult compRes = IndexCompactor::compact(catalog, index, compCat, compIdx);
    TEST_ASSERT(compRes.isSuccess());
    TEST_ASSERT_EQ(compRes.documentCount, 3U);
    TEST_ASSERT_EQ(compRes.activeDocumentCount, 3U);

    // 2. Validate Compacted Document Catalog
    TEST_ASSERT_EQ(compCat.totalCount(), 3);
    TEST_ASSERT_EQ(compCat.activeCount(), 3);

    // Old Doc 1 -> New Doc 1
    TEST_ASSERT_EQ(compCat.getPath(1), std::string("/file1.txt"));
    TEST_ASSERT(compCat.isActive(1));

    // Old Doc 3 -> New Doc 2
    TEST_ASSERT_EQ(compCat.getPath(2), std::string("/file3.txt"));
    TEST_ASSERT(compCat.isActive(2));

    // Old Doc 5 -> New Doc 3
    TEST_ASSERT_EQ(compCat.getPath(3), std::string("/file5.txt"));
    TEST_ASSERT(compCat.isActive(3));

    // Path lookup maps to new IDs
    TEST_ASSERT_EQ(compCat.findDocIdByPath("/file1.txt"), 1U);
    TEST_ASSERT_EQ(compCat.findDocIdByPath("/file3.txt"), 2U);
    TEST_ASSERT_EQ(compCat.findDocIdByPath("/file5.txt"), 3U);
    TEST_ASSERT_EQ(compCat.findDocIdByPath("/file2.txt"), INVALID_DOC_ID);
    TEST_ASSERT_EQ(compCat.findDocIdByPath("/file4.txt"), INVALID_DOC_ID);

    // 3. Validate Remapped Inverted Index Postings
    const dsa::CustomPostingList* postCommon = compIdx.getPostingList("common");
    TEST_ASSERT(postCommon != nullptr);
    TEST_ASSERT_EQ(postCommon->documentFrequency(), 3);
    TEST_ASSERT(postCommon->containsDocId(1));
    TEST_ASSERT(postCommon->containsDocId(2));
    TEST_ASSERT(postCommon->containsDocId(3));
    TEST_ASSERT(!postCommon->containsDocId(4));
    TEST_ASSERT(!postCommon->containsDocId(5));

    // "deleted_only" belonged only to Docs 2 and 4 -> must have 0 postings or be absent
    const dsa::CustomPostingList* postDeleted = compIdx.getPostingList("deleted_only");
    TEST_ASSERT(postDeleted == nullptr || postDeleted->empty());

    // "live_term" belonged to old Doc 3 -> must now belong to new Doc 2
    const dsa::CustomPostingList* postLive = compIdx.getPostingList("live_term");
    TEST_ASSERT(postLive != nullptr);
    TEST_ASSERT_EQ(postLive->documentFrequency(), 1);
    TEST_ASSERT(postLive->containsDocId(2));
    TEST_ASSERT(!postLive->containsDocId(3));

    // 4. Validate Remapped Document Length Statistics
    TEST_ASSERT_EQ(compIdx.documentLengthStatistics().activeDocumentCount(), 3U);
    TEST_ASSERT_EQ(compIdx.documentLengthStatistics().getBodyLength(1), 10U); // Old Doc 1
    TEST_ASSERT_EQ(compIdx.documentLengthStatistics().getBodyLength(2), 30U); // Old Doc 3
    TEST_ASSERT_EQ(compIdx.documentLengthStatistics().getBodyLength(3), 50U); // Old Doc 5
}

} // namespace sse::test
