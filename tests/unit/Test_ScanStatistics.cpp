#include "TestHarness.hpp"
#include "indexing/ScanStatistics.hpp"
#include <thread>
#include <chrono>

namespace sse::test {

void runScanStatisticsTests() {
    std::cout << "[RUNNING] ScanStatistics Tests..." << std::endl;

    using namespace sse::indexing;

    // Test 1: Initial State & Reset
    {
        ScanStatistics stats;
        TEST_ASSERT_EQ(stats.filesDiscovered, 0ULL);
        TEST_ASSERT_EQ(stats.directoriesScanned, 0ULL);
        TEST_ASSERT(!stats.isCompleted);
        TEST_ASSERT(!stats.isCancelled);
    }

    // Test 2: Start & Stop with Duration
    {
        ScanStatistics stats;
        stats.start();

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        stats.filesDiscovered = 100;
        stats.directoriesScanned = 10;
        stats.stop(false);

        TEST_ASSERT(stats.isCompleted);
        TEST_ASSERT(!stats.isCancelled);
        TEST_ASSERT(stats.elapsedMilliseconds >= 15);
        TEST_ASSERT(stats.filesPerSecond() > 0.0);
        TEST_ASSERT(stats.directoriesPerSecond() > 0.0);
    }

    // Test 3: Cancellation Flag Recording
    {
        ScanStatistics stats;
        stats.start();
        stats.stop(true);

        TEST_ASSERT(!stats.isCompleted);
        TEST_ASSERT(stats.isCancelled);
    }
}

} // namespace sse::test
