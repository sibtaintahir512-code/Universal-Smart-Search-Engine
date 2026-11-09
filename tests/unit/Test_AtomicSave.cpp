#include "TestHarness.hpp"
#include "persistence/IndexPersistenceManager.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include <filesystem>

namespace sse::test {

void runAtomicSaveTests() {
    std::cout << "[RUNNING] AtomicSave Tests..." << std::endl;

    using namespace sse::indexing;
    using namespace sse::persistence;

    std::filesystem::path testDir = "build/test_atomic_save_dir";
    std::error_code ec;
    std::filesystem::remove_all(testDir, ec);

    DocumentCatalog catalog;
    InvertedIndex index;

    FileMetadata m1{};
    m1.fileSize = 512;
    DocID d1 = catalog.registerDocument(m1, "/file1.txt", "file1.txt");
    index.addTerm("database", d1, 1, 3, FIELD_FILENAME | FIELD_CONTENT);
    index.documentLengthStatistics().recordDocumentLength(d1, 1, 20);

    IndexPersistenceManager manager(&index, &catalog, testDir);

    // 1. First Save: creates search.idx, no backup yet
    PersistenceResult res1 = manager.saveIndex();
    TEST_ASSERT(res1.isSuccess());
    TEST_ASSERT(std::filesystem::exists(testDir / "search.idx", ec));
    TEST_ASSERT(!std::filesystem::exists(testDir / "search.idx.tmp", ec)); // Temp must be unlinked/committed
    TEST_ASSERT_EQ(manager.statistics().totalSaves(), 1U);
    TEST_ASSERT(manager.generation() > 1);

    // 2. Second Save: creates search.idx.bak and advances generation
    PersistenceResult res2 = manager.saveIndex();
    TEST_ASSERT(res2.isSuccess());
    TEST_ASSERT(std::filesystem::exists(testDir / "search.idx", ec));
    TEST_ASSERT(std::filesystem::exists(testDir / "search.idx.bak", ec)); // Previous copy preserved as backup
    TEST_ASSERT(!std::filesystem::exists(testDir / "search.idx.tmp", ec));
    TEST_ASSERT_EQ(manager.statistics().totalSaves(), 2U);

    // 3. Load verification from disk
    DocumentCatalog loadedCat;
    InvertedIndex loadedIdx;
    IndexPersistenceManager loadManager(&loadedIdx, &loadedCat, testDir);

    PersistenceResult loadRes = loadManager.loadIndex();
    TEST_ASSERT(loadRes.isSuccess());
    TEST_ASSERT_EQ(loadedCat.totalCount(), 1);
    TEST_ASSERT_EQ(loadedIdx.vocabularySize(), 1);
    TEST_ASSERT_EQ(loadedIdx.totalPostingsCount(), 1);
    TEST_ASSERT_EQ(loadManager.statistics().totalLoads(), 1U);

    // Cleanup
    std::filesystem::remove_all(testDir, ec);
}

} // namespace sse::test
