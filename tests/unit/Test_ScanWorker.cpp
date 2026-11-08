#include "TestHarness.hpp"
#include "indexing/ScanWorker.hpp"
#include "indexing/DocumentCatalog.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>

namespace sse::test {

void runScanWorkerTests() {
    std::cout << "[RUNNING] ScanWorker Tests..." << std::endl;

    using namespace sse::indexing;

    // Test 1: Asynchronous Background Scan & Completion
    {
        auto root = std::filesystem::temp_directory_path() / "sse_worker_test";
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "sub");

        for (int i = 0; i < 50; ++i) {
            std::ofstream(root / ("file_" + std::to_string(i) + ".txt")) << "sample content";
        }

        DocumentCatalog catalog;
        ScanWorker worker;

        std::atomic<bool> completed{false};
        std::atomic<uint64_t> reportedFiles{0};

        bool started = worker.startScan(
            {root.u8string()},
            catalog,
            ScannerConfig(),
            [&reportedFiles](const ScanStatistics& s) {
                reportedFiles.store(s.filesDiscovered);
            },
            [&completed](const ScanStatistics& s) {
                completed.store(s.isCompleted);
            }
        );

        TEST_ASSERT(started);
        worker.wait(); // Wait for completion

        TEST_ASSERT(completed.load());
        TEST_ASSERT_EQ(catalog.totalCount(), 50);

        std::filesystem::remove_all(root);
    }

    // Test 2: Cooperative Cancellation Midway
    {
        auto root = std::filesystem::temp_directory_path() / "sse_cancel_test";
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);

        // Generate 500 files to allow cancellation midway
        for (int i = 0; i < 500; ++i) {
            std::ofstream(root / ("cancel_test_" + std::to_string(i) + ".txt")) << "data";
        }

        DocumentCatalog catalog;
        ScanWorker worker;

        bool started = worker.startScan({root.u8string()}, catalog);
        TEST_ASSERT(started);

        // Immediate cancellation
        worker.cancel();
        worker.wait();

        ScanStatistics finalStats = worker.getStatistics();
        TEST_ASSERT(finalStats.isCancelled);
        TEST_ASSERT(!finalStats.isCompleted);

        // Important: In a cancelled scan, missing files must NOT be reconciled!
        TEST_ASSERT_EQ(finalStats.missingFiles, 0ULL);

        std::filesystem::remove_all(root);
    }

    // Test 3: Large Directory Stress Test (1,000 files)
    {
        auto root = std::filesystem::temp_directory_path() / "sse_stress_1000";
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);

        for (int i = 0; i < 1000; ++i) {
            std::ofstream(root / ("f_" + std::to_string(i) + ".log")) << "log data";
        }

        DocumentCatalog catalog;
        ScanWorker worker;

        worker.startScan({root.u8string()}, catalog);
        worker.wait();

        ScanStatistics stats = worker.getStatistics();
        TEST_ASSERT(stats.isCompleted);
        TEST_ASSERT_EQ(stats.filesCataloged, 1000ULL);
        TEST_ASSERT_EQ(catalog.activeCount(), 1000);

        std::filesystem::remove_all(root);
    }
}

} // namespace sse::test
