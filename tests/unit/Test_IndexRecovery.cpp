#include "TestHarness.hpp"
#include "persistence/IndexPersistenceManager.hpp"
#include "persistence/IndexRecoveryManager.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include <filesystem>
#include <fstream>

namespace sse::test {

void runIndexRecoveryTests() {
    std::cout << "[RUNNING] IndexRecovery Tests..." << std::endl;

    using namespace sse::indexing;
    using namespace sse::persistence;

    std::filesystem::path testDir = "build/test_recovery_dir";
    std::error_code ec;
    std::filesystem::remove_all(testDir, ec);

    DocumentCatalog catalog;
    InvertedIndex index;

    FileMetadata m1{};
    m1.fileSize = 1000;
    DocID d1 = catalog.registerDocument(m1, "/doc.txt", "doc.txt");
    index.addTerm("network", d1, 1, 1, FIELD_FILENAME);
    index.documentLengthStatistics().recordDocumentLength(d1, 1, 10);

    IndexPersistenceManager manager(&index, &catalog, testDir);
    // Create primary and backup
    manager.saveIndex();
    manager.saveIndex();

    TEST_ASSERT(std::filesystem::exists(testDir / "search.idx", ec));
    TEST_ASSERT(std::filesystem::exists(testDir / "search.idx.bak", ec));

    // 1. Recover with intact primary
    {
        DocumentCatalog recCat;
        InvertedIndex recIdx;
        PersistenceResult res = IndexRecoveryManager::recover(testDir, "search.idx", recCat, recIdx);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT_EQ(res.status, PersistenceStatus::Success);
        TEST_ASSERT_EQ(recCat.totalCount(), 1);
    }

    // 2. Corrupt Primary -> Must Failover to Backup
    {
        // Overwrite search.idx with garbage bytes
        {
            std::ofstream badFile(testDir / "search.idx", std::ios::binary | std::ios::trunc);
            badFile << "THIS IS TOTALLY CORRUPTED AND INVALID HEADER DATA!";
        }

        DocumentCatalog recCat;
        InvertedIndex recIdx;
        PersistenceResult res = IndexRecoveryManager::recover(testDir, "search.idx", recCat, recIdx);

        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT_EQ(res.status, PersistenceStatus::RecoveredFromBackup);
        TEST_ASSERT_EQ(recCat.totalCount(), 1);
        TEST_ASSERT(recIdx.containsTerm("network"));

        // Verify corrupted file was isolated as search.idx.corrupt
        TEST_ASSERT(std::filesystem::exists(testDir / "search.idx.corrupt", ec));
    }

    // 3. Both Primary and Backup Missing
    {
        std::filesystem::path emptyDir = "build/test_recovery_empty_dir";
        std::filesystem::remove_all(emptyDir, ec);
        std::filesystem::create_directories(emptyDir, ec);

        DocumentCatalog recCat;
        InvertedIndex recIdx;
        PersistenceResult res = IndexRecoveryManager::recover(emptyDir, "search.idx", recCat, recIdx);
        TEST_ASSERT(!res.isSuccess());
        TEST_ASSERT_EQ(res.status, PersistenceStatus::FileNotFound);

        std::filesystem::remove_all(emptyDir, ec);
    }

    // Cleanup
    std::filesystem::remove_all(testDir, ec);
}

} // namespace sse::test
