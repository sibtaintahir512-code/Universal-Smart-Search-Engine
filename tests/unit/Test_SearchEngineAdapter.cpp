#include "TestHarness.hpp"
#include "gui/adapters/SearchEngineAdapter.hpp"
#include <filesystem>

namespace sse::test {

void runSearchEngineAdapterTests() {
    std::cout << "\n=== Running SearchEngineAdapter Unit Tests ===" << std::endl;

    std::filesystem::path testDir = "test_gui_adapter_data";
    std::filesystem::remove_all(testDir);
    std::filesystem::create_directories(testDir);

    // Test 1: Adapter construction & empty telemetry
    {
        gui::SearchEngineAdapter adapter(testDir);
        auto t = adapter.getTelemetry();
        TEST_ASSERT_EQ(t.totalCatalogDocuments, 0);
        TEST_ASSERT_EQ(t.activeCatalogDocuments, 0);
        TEST_ASSERT_EQ(t.vocabularyTerms, 0);
        TEST_ASSERT_EQ(t.persistenceGeneration, 1);
    }

    // Test 2: Live Query Validation
    {
        gui::SearchEngineAdapter adapter(testDir);
        std::string err;
        std::size_t offset = 0;

        // Valid simple query
        TEST_ASSERT(adapter.validateQuery("algorithm", err, offset));
        TEST_ASSERT(err.empty());

        // Valid complex Boolean query
        TEST_ASSERT(adapter.validateQuery("(dijkstra OR astar) AND NOT graph", err, offset));

        // Empty query
        TEST_ASSERT(!adapter.validateQuery("", err, offset));
        TEST_ASSERT(!err.empty());

        // Invalid query: unbalanced parentheses
        TEST_ASSERT(!adapter.validateQuery("(dijkstra OR astar", err, offset));
        TEST_ASSERT(!err.empty());
    }

    // Test 3: Autocomplete suggestions via CustomTrie
    {
        gui::SearchEngineAdapter adapter(testDir);
        // Suggestions on empty index should be empty
        auto s1 = adapter.getAutocompleteSuggestions("dijk", 5);
        TEST_ASSERT_EQ(s1.size(), 0);

        // Telemetry check
        auto t = adapter.getTelemetry();
        TEST_ASSERT_EQ(t.totalQueriesExecuted, 0);
    }

    // Test 4: Search on empty index
    {
        gui::SearchEngineAdapter adapter(testDir);
        auto resp = adapter.executeSearch("anything");
        TEST_ASSERT_EQ(static_cast<int>(resp.status), static_cast<int>(search::SearchStatus::Success));
        TEST_ASSERT_EQ(resp.totalMatches, 0);
        TEST_ASSERT_EQ(resp.results.size(), 0);

        auto t = adapter.getTelemetry();
        TEST_ASSERT_EQ(t.totalQueriesExecuted, 1);
    }

    // Test 5: Save and Load empty index via adapter
    {
        gui::SearchEngineAdapter adapter(testDir);
        auto saveRes = adapter.saveIndex("test_adapter.idx");
        TEST_ASSERT(saveRes.isSuccess());

        auto loadRes = adapter.loadIndex("test_adapter.idx");
        TEST_ASSERT(loadRes.isSuccess());
    }

    std::filesystem::remove_all(testDir);
}

} // namespace sse::test
