#include "TestHarness.hpp"
#include "indexing/FileChangeDetector.hpp"
#include "indexing/DocumentCatalog.hpp"
#include <fstream>
#include <filesystem>

namespace sse::test {

void runFileChangeDetectorTests() {
    std::cout << "[RUNNING] FileChangeDetector Tests..." << std::endl;

    using namespace sse::indexing;

    // Test 1: Status Transitions
    {
        DocumentCatalog catalog;

        // Path not in catalog, exists on disk -> New
        FileChangeStatus s1 = FileChangeDetector::detectChange(
            catalog, "C:/newfile.txt", 100, 1000, true
        );
        TEST_ASSERT_EQ(s1, FileChangeStatus::New);

        // Path not in catalog, does not exist -> Error
        FileChangeStatus s2 = FileChangeDetector::detectChange(
            catalog, "C:/ghost.txt", 0, 0, false
        );
        TEST_ASSERT_EQ(s2, FileChangeStatus::Error);

        // Register file
        FileMetadata meta;
        meta.fileSize = 500;
        meta.lastModifiedTime = 1000;
        catalog.registerDocument(meta, "C:/existing.txt", "existing.txt");

        // Same size and modTime -> Unchanged
        FileChangeStatus s3 = FileChangeDetector::detectChange(
            catalog, "C:/existing.txt", 500, 1000, true
        );
        TEST_ASSERT_EQ(s3, FileChangeStatus::Unchanged);

        // Modified size -> Modified
        FileChangeStatus s4 = FileChangeDetector::detectChange(
            catalog, "C:/existing.txt", 600, 1000, true
        );
        TEST_ASSERT_EQ(s4, FileChangeStatus::Modified);

        // Modified time -> Modified
        FileChangeStatus s5 = FileChangeDetector::detectChange(
            catalog, "C:/existing.txt", 500, 1050, true
        );
        TEST_ASSERT_EQ(s5, FileChangeStatus::Modified);

        // File missing on disk -> Missing
        FileChangeStatus s6 = FileChangeDetector::detectChange(
            catalog, "C:/existing.txt", 0, 0, false
        );
        TEST_ASSERT_EQ(s6, FileChangeStatus::Missing);
    }

    // Test 2: Reconcile Missing Documents with real files
    {
        auto tempDir = std::filesystem::temp_directory_path() / "sse_change_detector_test";
        std::filesystem::create_directories(tempDir);

        auto fileA = tempDir / "fileA.txt";
        auto fileB = tempDir / "fileB.txt";
        auto fileC = tempDir / "fileC.txt";

        { std::ofstream(fileA) << "AAA"; }
        { std::ofstream(fileB) << "BBB"; }
        { std::ofstream(fileC) << "CCC"; }

        DocumentCatalog catalog;
        FileMetadata m;
        DocID idA = catalog.registerDocument(m, fileA.u8string(), "fileA.txt");
        DocID idB = catalog.registerDocument(m, fileB.u8string(), "fileB.txt");
        DocID idC = catalog.registerDocument(m, fileC.u8string(), "fileC.txt");

        TEST_ASSERT_EQ(catalog.activeCount(), 3);

        // Now physically delete fileB from disk
        std::filesystem::remove(fileB);

        // Current scan only discovered fileA and fileC
        dsa::CustomDynamicArray<std::string> discovered;
        discovered.push_back(fileA.u8string());
        discovered.push_back(fileC.u8string());

        std::size_t missingCount = FileChangeDetector::reconcileMissingDocuments(catalog, discovered);
        TEST_ASSERT_EQ(missingCount, 1);
        TEST_ASSERT(catalog.isActive(idA));
        TEST_ASSERT(!catalog.isActive(idB)); // fileB tombstoned
        TEST_ASSERT(catalog.isActive(idC));
        TEST_ASSERT_EQ(catalog.activeCount(), 2);

        // Clean up
        std::filesystem::remove_all(tempDir);
    }
}

} // namespace sse::test
