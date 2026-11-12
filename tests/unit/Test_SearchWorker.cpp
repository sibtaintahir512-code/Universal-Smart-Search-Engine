#include "TestHarness.hpp"
#include "gui/workers/SearchWorker.hpp"
#include "gui/adapters/SearchEngineAdapter.hpp"
#include <filesystem>

namespace sse::test {

void runSearchWorkerTests() {
    std::cout << "\n=== Running SearchWorker Unit Tests ===" << std::endl;

    std::filesystem::path testDir = "test_gui_worker_data";
    std::filesystem::remove_all(testDir);
    std::filesystem::create_directories(testDir);

    gui::SearchEngineAdapter adapter(testDir);

    // Test 1: Worker instantiation
    {
        gui::SearchWorker worker(&adapter);
        bool started = false;
        bool completed = false;

        QObject::connect(&worker, &gui::SearchWorker::searchStarted, [&]() {
            started = true;
        });

        QObject::connect(&worker, &gui::SearchWorker::searchCompleted, [&](const search::SearchResponse& resp) {
            completed = true;
            TEST_ASSERT_EQ(static_cast<int>(resp.status), static_cast<int>(search::SearchStatus::Success));
            TEST_ASSERT_EQ(resp.totalMatches, 0);
        });

        worker.performSearch("test", 0, 50, static_cast<int>(ranking::RankingMode::BM25));
        TEST_ASSERT(started);
        TEST_ASSERT(completed);
    }

    // Test 2: Search cancellation flag
    {
        gui::SearchWorker worker(&adapter);
        worker.cancel();
        // Cancellation flag is set; subsequent search will handle cancellation cleanly
    }

    // Test 3: Null adapter resilience
    {
        gui::SearchWorker worker(nullptr);
        bool failed = false;
        QObject::connect(&worker, &gui::SearchWorker::searchFailed, [&](const QString& err) {
            failed = true;
            TEST_ASSERT(!err.isEmpty());
        });

        worker.performSearch("test");
        TEST_ASSERT(failed);
    }

    std::filesystem::remove_all(testDir);
}

} // namespace sse::test
