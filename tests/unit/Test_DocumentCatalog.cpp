#include "TestHarness.hpp"
#include "indexing/DocumentCatalog.hpp"

namespace sse::test {

void runDocumentCatalogTests() {
    std::cout << "[RUNNING] DocumentCatalog Tests..." << std::endl;

    using namespace sse::indexing;

    // Test 1: Empty Catalog State
    {
        DocumentCatalog catalog;
        TEST_ASSERT_EQ(catalog.totalCount(), 0);
        TEST_ASSERT_EQ(catalog.activeCount(), 0);
        TEST_ASSERT(catalog.getMetadata(1) == nullptr);
        TEST_ASSERT_EQ(catalog.getPath(1), std::string(""));
        TEST_ASSERT_EQ(catalog.findDocIdByPath("C:/test.txt"), INVALID_DOC_ID);
    }

    // Test 2: Monotonic Registration & String Pool Extraction
    {
        DocumentCatalog catalog;

        FileMetadata m1;
        m1.fileSize = 1024;
        m1.lastModifiedTime = 1600000000;
        m1.category = FileCategory::Documents;

        FileMetadata m2;
        m2.fileSize = 2048;
        m2.lastModifiedTime = 1600000500;
        m2.category = FileCategory::Images;

        DocID id1 = catalog.registerDocument(m1, "C:/docs/report.pdf", "report.pdf");
        DocID id2 = catalog.registerDocument(m2, "C:/images/photo.png", "photo.png");

        TEST_ASSERT_EQ(id1, 1);
        TEST_ASSERT_EQ(id2, 2);
        TEST_ASSERT_EQ(catalog.totalCount(), 2);
        TEST_ASSERT_EQ(catalog.activeCount(), 2);

        TEST_ASSERT_EQ(catalog.getPath(id1), std::string("C:/docs/report.pdf"));
        TEST_ASSERT_EQ(catalog.getFilename(id1), std::string("report.pdf"));
        TEST_ASSERT_EQ(catalog.getPath(id2), std::string("C:/images/photo.png"));
        TEST_ASSERT_EQ(catalog.getFilename(id2), std::string("photo.png"));

        TEST_ASSERT_EQ(catalog.findDocIdByPath("C:/docs/report.pdf"), 1);
        TEST_ASSERT_EQ(catalog.findDocIdByPath("C:/images/photo.png"), 2);
        TEST_ASSERT_EQ(catalog.findDocIdByPath("C:/nonexistent.txt"), INVALID_DOC_ID);
    }

    // Test 3: Duplicate Path Registration & Incremental Update
    {
        DocumentCatalog catalog;
        FileMetadata m;
        m.fileSize = 100;
        m.lastModifiedTime = 1000;

        DocID idFirst = catalog.registerDocument(m, "C:/work/code.cpp", "code.cpp");
        TEST_ASSERT_EQ(idFirst, 1);
        TEST_ASSERT_EQ(catalog.totalCount(), 1);

        // Re-register same canonical path with modified size and timestamp
        m.fileSize = 150;
        m.lastModifiedTime = 2000;
        DocID idSecond = catalog.registerDocument(m, "C:/work/code.cpp", "code.cpp");

        // Must preserve the same DocID without creating a duplicate record
        TEST_ASSERT_EQ(idSecond, 1);
        TEST_ASSERT_EQ(catalog.totalCount(), 1);

        const FileMetadata* updated = catalog.getMetadata(idSecond);
        TEST_ASSERT(updated != nullptr);
        TEST_ASSERT_EQ(updated->fileSize, 150ULL);
        TEST_ASSERT_EQ(updated->lastModifiedTime, 2000ULL);
        TEST_ASSERT((updated->flags & META_FLAG_MODIFIED) != 0);
    }

    // Test 4: Tombstone Deletion & Reactivation
    {
        DocumentCatalog catalog;
        FileMetadata m;
        DocID id = catalog.registerDocument(m, "C:/temp/junk.tmp", "junk.tmp");
        TEST_ASSERT(catalog.isActive(id));
        TEST_ASSERT_EQ(catalog.activeCount(), 1);

        // Mark deleted
        TEST_ASSERT(catalog.markDeleted(id));
        TEST_ASSERT(!catalog.isActive(id));
        TEST_ASSERT_EQ(catalog.activeCount(), 0);
        TEST_ASSERT_EQ(catalog.totalCount(), 1); // DocID is NOT recycled!

        // Reactivate
        TEST_ASSERT(catalog.reactivate(id));
        TEST_ASSERT(catalog.isActive(id));
        TEST_ASSERT_EQ(catalog.activeCount(), 1);
    }

    // Test 5: Clear Catalog
    {
        DocumentCatalog catalog;
        FileMetadata m;
        catalog.registerDocument(m, "C:/file1.txt", "file1.txt");
        catalog.registerDocument(m, "C:/file2.txt", "file2.txt");

        TEST_ASSERT_EQ(catalog.totalCount(), 2);
        catalog.clear();
        TEST_ASSERT_EQ(catalog.totalCount(), 0);
        TEST_ASSERT_EQ(catalog.activeCount(), 0);
    }
}

} // namespace sse::test
