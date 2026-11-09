#include "TestHarness.hpp"
#include "indexing/IndexingPipeline.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include "extraction/ExtractionResult.hpp"

namespace sse::test {

void runIndexingPipelineTests() {
    std::cout << "[RUNNING] IndexingPipeline Tests..." << std::endl;

    using namespace sse::indexing;
    using namespace sse::extraction;
    using sse::extraction::ExtractionStatus;

    // Helper: Register a document in catalog
    auto registerDoc = [](DocumentCatalog& cat, const std::string& path, const std::string& filename) -> DocID {
        FileMetadata meta;
        meta.fileSize = 1024;
        meta.category = FileCategory::Documents;
        return cat.registerDocument(meta, path, filename);
    };

    // Test 1: Successful Full Text Extraction and Indexing
    {
        DocumentCatalog catalog;
        InvertedIndex index;
        IndexingPipeline pipeline(catalog, index);

        DocID d1 = registerDoc(catalog, "/docs/report.txt", "report.txt");
        TEST_ASSERT(d1 != INVALID_DOC_ID);

        ExtractionResult ext;
        ext.docId = d1;
        ext.status = ExtractionStatus::Success;
        ext.extractedText = "The smart search engine indexes documents efficiently.";

        IndexingResult res = pipeline.indexDocument(d1, &ext);
        TEST_ASSERT(res.success);
        TEST_ASSERT(!res.isPartial);
        TEST_ASSERT(!res.filenameOnly);
        TEST_ASSERT(res.tokensIndexed > 0);

        // Filename term "report" should be in index with FIELD_FILENAME
        const dsa::CustomPostingList* pReport = index.getPostingList("report");
        TEST_ASSERT(pReport != nullptr);
        TEST_ASSERT_EQ(pReport->head()->docId, d1);
        TEST_ASSERT(pReport->head()->fieldMask & FIELD_FILENAME);
        TEST_ASSERT_EQ(pReport->head()->nameTermFreq, 1U);
        TEST_ASSERT_EQ(pReport->head()->bodyTermFreq, 0U);

        // Body term "smart" should be in index with FIELD_CONTENT
        const dsa::CustomPostingList* pSmart = index.getPostingList("smart");
        TEST_ASSERT(pSmart != nullptr);
        TEST_ASSERT_EQ(pSmart->head()->docId, d1);
        TEST_ASSERT(pSmart->head()->fieldMask & FIELD_CONTENT);
        TEST_ASSERT_EQ(pSmart->head()->nameTermFreq, 0U);
        TEST_ASSERT_EQ(pSmart->head()->bodyTermFreq, 1U);

        // Statistics check
        TEST_ASSERT_EQ(pipeline.statistics().documentsFullyIndexed, 1U);
        TEST_ASSERT_EQ(pipeline.statistics().documentsProcessed, 1U);
    }

    // Test 2: Partial Content Extraction (Output Budget Reached)
    {
        DocumentCatalog catalog;
        InvertedIndex index;
        IndexingPipeline pipeline(catalog, index);

        DocID d1 = registerDoc(catalog, "/docs/large.pdf", "large.pdf");
        ExtractionResult ext;
        ext.docId = d1;
        ext.status = ExtractionStatus::Partial;
        ext.isTruncated = true;
        ext.extractedText = "Chapter one truncated text";

        IndexingResult res = pipeline.indexDocument(d1, &ext);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.isPartial);
        TEST_ASSERT(!res.filenameOnly);

        TEST_ASSERT(index.containsTerm("large"));
        TEST_ASSERT(index.containsTerm("chapter"));
        TEST_ASSERT_EQ(pipeline.statistics().documentsPartiallyIndexed, 1U);
    }

    // Test 3: Metadata-Only, Corrupted, and Encrypted Files (Filename-Only Indexing)
    {
        DocumentCatalog catalog;
        InvertedIndex index;
        IndexingPipeline pipeline(catalog, index);

        // MetadataOnly (e.g. image or archive)
        DocID dImg = registerDoc(catalog, "/images/photo_vacation.jpg", "photo_vacation.jpg");
        ExtractionResult extImg;
        extImg.docId = dImg;
        extImg.status = ExtractionStatus::MetadataOnly;

        IndexingResult resImg = pipeline.indexDocument(dImg, &extImg);
        TEST_ASSERT(resImg.success);
        TEST_ASSERT(resImg.filenameOnly);
        TEST_ASSERT(index.containsTerm("photo"));
        TEST_ASSERT(index.containsTerm("vacation"));

        // Corrupted file
        DocID dCorrupt = registerDoc(catalog, "/docs/broken.docx", "broken.docx");
        ExtractionResult extCorrupt;
        extCorrupt.docId = dCorrupt;
        extCorrupt.status = ExtractionStatus::Corrupted;

        IndexingResult resCorrupt = pipeline.indexDocument(dCorrupt, &extCorrupt);
        TEST_ASSERT(resCorrupt.success);
        TEST_ASSERT(resCorrupt.filenameOnly);
        TEST_ASSERT(index.containsTerm("broken"));

        // Encrypted PDF
        DocID dEnc = registerDoc(catalog, "/docs/confidential.pdf", "confidential.pdf");
        ExtractionResult extEnc;
        extEnc.docId = dEnc;
        extEnc.status = ExtractionStatus::Encrypted;

        IndexingResult resEnc = pipeline.indexDocument(dEnc, &extEnc);
        TEST_ASSERT(resEnc.success);
        TEST_ASSERT(resEnc.filenameOnly);
        TEST_ASSERT(index.containsTerm("confidential"));

        TEST_ASSERT_EQ(pipeline.statistics().documentsFilenameOnly, 3U);
    }

    // Test 4: Cancelled Extraction (Do not commit, return cancelled)
    {
        DocumentCatalog catalog;
        InvertedIndex index;
        IndexingPipeline pipeline(catalog, index);

        DocID d1 = registerDoc(catalog, "/docs/cancelled.txt", "cancelled.txt");
        ExtractionResult ext;
        ext.docId = d1;
        ext.status = ExtractionStatus::Cancelled;
        ext.extractedText = "Partial text that should not be committed";

        IndexingResult res = pipeline.indexDocument(d1, &ext);
        TEST_ASSERT(!res.success);
        TEST_ASSERT(!index.containsTerm("cancelled"));
        TEST_ASSERT(!index.containsTerm("committed"));
        TEST_ASSERT_EQ(pipeline.statistics().documentsFailed, 1U);
    }

    // Test 5: Invalid DocID and Inactive Document
    {
        DocumentCatalog catalog;
        InvertedIndex index;
        IndexingPipeline pipeline(catalog, index);

        // INVALID_DOC_ID (0)
        IndexingResult res0 = pipeline.indexDocument(INVALID_DOC_ID);
        TEST_ASSERT(!res0.success);

        // Non-registered DocID
        IndexingResult res99 = pipeline.indexDocument(999);
        TEST_ASSERT(!res99.success);
    }

    // Test 6: Document Update Without Stale Postings
    {
        DocumentCatalog catalog;
        InvertedIndex index;
        IndexingPipeline pipeline(catalog, index);

        DocID d1 = registerDoc(catalog, "/docs/article.txt", "article.txt");

        // Initial indexing: "apple banana"
        ExtractionResult ext1;
        ext1.docId = d1;
        ext1.status = ExtractionStatus::Success;
        ext1.extractedText = "apple banana";
        pipeline.indexDocument(d1, &ext1);

        TEST_ASSERT(index.containsTerm("apple"));
        TEST_ASSERT(index.containsTerm("banana"));

        // Update document: completely new content "cherry orange"
        ExtractionResult ext2;
        ext2.docId = d1;
        ext2.status = ExtractionStatus::Success;
        ext2.extractedText = "cherry orange";
        IndexingResult resUp = pipeline.updateDocument(d1, &ext2);
        TEST_ASSERT(resUp.success);

        // Old terms MUST NOT remain searchable!
        TEST_ASSERT(!index.containsTerm("apple"));
        TEST_ASSERT(!index.containsTerm("banana"));

        // New terms MUST be searchable
        TEST_ASSERT(index.containsTerm("cherry"));
        TEST_ASSERT(index.containsTerm("orange"));
        TEST_ASSERT(index.containsTerm("article")); // Filename still present
    }

    // Test 7: Document Removal
    {
        DocumentCatalog catalog;
        InvertedIndex index;
        IndexingPipeline pipeline(catalog, index);

        DocID d1 = registerDoc(catalog, "/docs/delete_me.txt", "delete_me.txt");
        ExtractionResult ext;
        ext.docId = d1;
        ext.status = ExtractionStatus::Success;
        ext.extractedText = "ephemeral content";
        pipeline.indexDocument(d1, &ext);

        TEST_ASSERT(index.containsTerm("ephemeral"));
        TEST_ASSERT(catalog.isActive(d1));

        bool removed = pipeline.removeDocument(d1);
        TEST_ASSERT(removed);
        TEST_ASSERT(!index.containsTerm("ephemeral"));
        TEST_ASSERT(!index.containsTerm("delete"));
        TEST_ASSERT(!catalog.isActive(d1)); // Marked tombstoned in catalog
    }
}

} // namespace sse::test
