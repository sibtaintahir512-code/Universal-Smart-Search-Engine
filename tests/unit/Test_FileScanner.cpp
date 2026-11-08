#include "TestHarness.hpp"
#include "indexing/FileScanner.hpp"
#include "indexing/DocumentCatalog.hpp"
#include <fstream>
#include <filesystem>

namespace sse::test {

void runFileScannerTests() {
    std::cout << "[RUNNING] FileScanner Tests..." << std::endl;

    using namespace sse::indexing;

    // Test 1: Ancestor Root Deduplication
    {
        std::vector<std::string> rawRoots = {
            "/tmp/test_root",
            "/tmp/test_root/subfolder",
            "/tmp/another_root"
        };
        auto deduped = FileScanner::deduplicateRoots(rawRoots);
        // /tmp/test_root/subfolder must be pruned because /tmp/test_root is already an ancestor
        TEST_ASSERT_EQ(deduped.size(), 2);
    }

    // Test 2: Real Filesystem BFS Traversal on Nested Directory Hierarchy
    {
        auto root = std::filesystem::temp_directory_path() / "sse_scanner_test_tree";
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "dir1" / "subdirA");
        std::filesystem::create_directories(root / "dir2");
        std::filesystem::create_directories(root / "empty_dir");

        // Files with various extensions, unicode names, spaces, no extension
        { std::ofstream(root / "file1.txt") << "file 1 content"; }
        { std::ofstream(root / "no_ext") << "no extension content"; }
        { std::ofstream(root / ".hidden") << "hidden file"; }
        { std::ofstream(root / "dir1" / "file2.cpp") << "int main() {}"; }
        { std::ofstream(root / "dir1" / "subdirA" / "deep_doc.pdf") << "pdf"; }
        { std::ofstream(root / "dir2" / "Special & File #1.txt") << "special chars"; }
        { std::ofstream(root / "dir2" / "اردو_فائل.txt") << "Urdu content"; }

        DocumentCatalog catalog;
        FileScanner scanner;
        scanner.addRoot(root.u8string());

        std::atomic<bool> cancelFlag{false};
        ScanStatistics stats = scanner.scan(catalog, nullptr, nullptr, cancelFlag);

        TEST_ASSERT(stats.isCompleted);
        TEST_ASSERT(!stats.isCancelled);
        TEST_ASSERT_EQ(stats.filesCataloged, 7ULL);
        TEST_ASSERT_EQ(catalog.activeCount(), 7);
        TEST_ASSERT_EQ(catalog.totalCount(), 7);

        // Verify unicode and special characters were cataloged
        std::error_code ecCanon;
        std::string specialPath = std::filesystem::weakly_canonical(root / "dir2" / "Special & File #1.txt", ecCanon).u8string();
        DocID sId = catalog.findDocIdByPath(specialPath);
        TEST_ASSERT_NE(sId, INVALID_DOC_ID);
        TEST_ASSERT_EQ(catalog.getFilename(sId), std::string("Special & File #1.txt"));

        // Cleanup
        std::filesystem::remove_all(root);
    }

    // Test 3: Invalid / Missing Root Directory Handling (Non-crashing)
    {
        DocumentCatalog catalog;
        FileScanner scanner;
        scanner.addRoot("/this/path/absolutely/does/not/exist_12345");

        std::atomic<bool> cancelFlag{false};
        ScanStatistics stats = scanner.scan(catalog, nullptr, nullptr, cancelFlag);

        TEST_ASSERT(stats.isCompleted);
        TEST_ASSERT_EQ(stats.filesDiscovered, 0ULL);
        TEST_ASSERT_EQ(stats.inaccessibleDirectories, 1ULL);
    }

    // Test 4: Symlink Cycle Suppression
    {
        auto root = std::filesystem::temp_directory_path() / "sse_scanner_symlink_test";
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "folderA");
        std::filesystem::create_directories(root / "folderB");

        { std::ofstream(root / "folderA" / "docA.txt") << "docA"; }

        std::error_code ec;
        // Create directory symlink: folderB/linkToA -> folderA
        std::filesystem::create_directory_symlink(root / "folderA", root / "folderB" / "linkToA", ec);

        DocumentCatalog catalog;
        ScannerConfig cfg;
        cfg.followSymlinks = false; // Default: do not follow symlinks
        FileScanner scanner(cfg);
        scanner.addRoot(root.u8string());

        std::atomic<bool> cancelFlag{false};
        ScanStatistics stats = scanner.scan(catalog, nullptr, nullptr, cancelFlag);

        TEST_ASSERT(stats.isCompleted);
        // docA is cataloged once, and symlink linkToA is safely skipped
        TEST_ASSERT_EQ(stats.filesCataloged, 1ULL);

        std::filesystem::remove_all(root);
    }
}

} // namespace sse::test
