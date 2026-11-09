#include "TestHarness.hpp"
#include "persistence/IndexSerializer.hpp"
#include "persistence/IndexDeserializer.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"

namespace sse::test {

void runIndexSerializationTests() {
    std::cout << "[RUNNING] IndexSerialization Tests..." << std::endl;

    using namespace sse::indexing;
    using namespace sse::persistence;

    // 1. Setup Test Catalog and Inverted Index
    DocumentCatalog catalog;
    InvertedIndex index;

    FileMetadata meta1{};
    meta1.fileSize = 1024;
    meta1.creationTime = 1000;
    meta1.lastModifiedTime = 2000;
    meta1.category = FileCategory::Documents;
    meta1.extractionState = ExtractionStatus::Extracted;

    DocID id1 = catalog.registerDocument(meta1, "/data/report.pdf", "report.pdf");

    FileMetadata meta2{};
    meta2.fileSize = 2048;
    meta2.creationTime = 3000;
    meta2.lastModifiedTime = 4000;
    meta2.category = FileCategory::SourceCode;
    meta2.extractionState = ExtractionStatus::Extracted;

    DocID id2 = catalog.registerDocument(meta2, "/src/main.cpp", "main.cpp");

    // Add postings
    // "algorithm" in both docs
    index.addTerm("algorithm", id1, 1, 5, FIELD_FILENAME | FIELD_CONTENT);
    index.addTerm("algorithm", id2, 0, 3, FIELD_CONTENT);

    // "compiler" in doc 2 only
    index.addTerm("compiler", id2, 1, 2, FIELD_FILENAME | FIELD_CONTENT);

    // Document length stats
    index.documentLengthStatistics().recordDocumentLength(id1, 1, 100);
    index.documentLengthStatistics().recordDocumentLength(id2, 1, 250);

    TEST_ASSERT_EQ(catalog.totalCount(), 2);
    TEST_ASSERT_EQ(catalog.activeCount(), 2);
    TEST_ASSERT_EQ(index.vocabularySize(), 2);
    TEST_ASSERT_EQ(index.totalPostingsCount(), 3);

    // 2. Serialize into BinaryWriter
    BinaryWriter writer;
    PersistenceResult serResult = IndexSerializer::serialize(catalog, index, writer, 1);

    TEST_ASSERT(serResult.isSuccess());
    TEST_ASSERT_EQ(serResult.documentCount, 2U);
    TEST_ASSERT_EQ(serResult.vocabularySize, 2U);
    TEST_ASSERT_EQ(serResult.postingsCount, 3ULL);
    TEST_ASSERT(writer.size() > INDEX_HEADER_SIZE);

    // 3. Dry-run validation
    PersistenceResult valResult = IndexDeserializer::validate(writer.data(), writer.size());
    TEST_ASSERT(valResult.isSuccess());

    // 4. Deserialize into clean, fresh Catalog and Index
    DocumentCatalog restoredCatalog;
    InvertedIndex restoredIndex;

    PersistenceResult desResult = IndexDeserializer::deserialize(
        writer.data(), writer.size(), restoredCatalog, restoredIndex);

    TEST_ASSERT(desResult.isSuccess());
    TEST_ASSERT_EQ(restoredCatalog.totalCount(), 2);
    TEST_ASSERT_EQ(restoredCatalog.activeCount(), 2);
    TEST_ASSERT_EQ(restoredIndex.vocabularySize(), 2);
    TEST_ASSERT_EQ(restoredIndex.totalPostingsCount(), 3);

    // 5. Verify Restored Document Catalog
    const FileMetadata* rMeta1 = restoredCatalog.getMetadata(id1);
    TEST_ASSERT(rMeta1 != nullptr);
    TEST_ASSERT_EQ(rMeta1->fileSize, 1024ULL);
    TEST_ASSERT_EQ(restoredCatalog.getPath(id1), std::string("/data/report.pdf"));
    TEST_ASSERT_EQ(restoredCatalog.getFilename(id1), std::string("report.pdf"));
    TEST_ASSERT(restoredCatalog.isActive(id1));

    const FileMetadata* rMeta2 = restoredCatalog.getMetadata(id2);
    TEST_ASSERT(rMeta2 != nullptr);
    TEST_ASSERT_EQ(rMeta2->fileSize, 2048ULL);
    TEST_ASSERT_EQ(restoredCatalog.getPath(id2), std::string("/src/main.cpp"));
    TEST_ASSERT_EQ(restoredCatalog.getFilename(id2), std::string("main.cpp"));
    TEST_ASSERT(restoredCatalog.isActive(id2));

    // Path lookup
    TEST_ASSERT_EQ(restoredCatalog.findDocIdByPath("/data/report.pdf"), id1);
    TEST_ASSERT_EQ(restoredCatalog.findDocIdByPath("/src/main.cpp"), id2);

    // 6. Verify Restored Inverted Index Postings
    const dsa::CustomPostingList* postAlg = restoredIndex.getPostingList("algorithm");
    TEST_ASSERT(postAlg != nullptr);
    TEST_ASSERT_EQ(postAlg->documentFrequency(), 2);

    const PostingNode* n1 = postAlg->findNode(id1);
    TEST_ASSERT(n1 != nullptr);
    TEST_ASSERT_EQ(n1->nameTermFreq, 1);
    TEST_ASSERT_EQ(n1->bodyTermFreq, 5);
    TEST_ASSERT_EQ(n1->fieldMask, FIELD_FILENAME | FIELD_CONTENT);

    const PostingNode* n2 = postAlg->findNode(id2);
    TEST_ASSERT(n2 != nullptr);
    TEST_ASSERT_EQ(n2->nameTermFreq, 0);
    TEST_ASSERT_EQ(n2->bodyTermFreq, 3);
    TEST_ASSERT_EQ(n2->fieldMask, FIELD_CONTENT);

    const dsa::CustomPostingList* postComp = restoredIndex.getPostingList("compiler");
    TEST_ASSERT(postComp != nullptr);
    TEST_ASSERT_EQ(postComp->documentFrequency(), 1);
    TEST_ASSERT(postComp->containsDocId(id2));
    TEST_ASSERT(!postComp->containsDocId(id1));

    // Trie synchronization
    TEST_ASSERT(restoredIndex.trie().contains("algorithm"));
    TEST_ASSERT(restoredIndex.trie().contains("compiler"));

    // 7. Verify Restored Document Lengths & BM25 Stats
    TEST_ASSERT_EQ(restoredIndex.documentLengthStatistics().getNameLength(id1), 1U);
    TEST_ASSERT_EQ(restoredIndex.documentLengthStatistics().getBodyLength(id1), 100U);
    TEST_ASSERT_EQ(restoredIndex.documentLengthStatistics().getNameLength(id2), 1U);
    TEST_ASSERT_EQ(restoredIndex.documentLengthStatistics().getBodyLength(id2), 250U);
    TEST_ASSERT_EQ(restoredIndex.documentLengthStatistics().totalBodyTokens(), 350ULL);
}

} // namespace sse::test
