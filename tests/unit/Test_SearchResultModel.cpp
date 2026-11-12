#include "TestHarness.hpp"
#include "gui/models/SearchResultModel.hpp"
#include "core/CommonTypes.hpp"
#include <QDateTime>

namespace sse::test {

void runSearchResultModelTests() {
    std::cout << "\n=== Running SearchResultModel Unit Tests ===" << std::endl;

    // Test 1: Empty model initial state
    {
        gui::SearchResultModel model;
        TEST_ASSERT_EQ(model.rowCount(), 0);
        TEST_ASSERT_EQ(model.columnCount(), gui::SearchResultModel::ColumnCount);
        TEST_ASSERT(model.getItem(0) == nullptr);
        TEST_ASSERT_EQ(model.totalUnfilteredCount(), 0);
        TEST_ASSERT_EQ(model.visibleCount(), 0);
    }

    // Test 2: Formatting utility helpers
    {
        TEST_ASSERT_EQ(gui::SearchResultModel::formatFileSize(500).toStdString(), "500 B");
        TEST_ASSERT_EQ(gui::SearchResultModel::formatFileSize(2048).toStdString(), "2.0 KB");
        TEST_ASSERT_EQ(gui::SearchResultModel::formatFileSize(5 * 1024 * 1024).toStdString(), "5.00 MB");
        TEST_ASSERT_EQ(gui::SearchResultModel::formatFileSize(3ULL * 1024 * 1024 * 1024).toStdString(), "3.00 GB");

        TEST_ASSERT_EQ(gui::SearchResultModel::formatCategory(FileCategory::Documents).toStdString(), "Document");
        TEST_ASSERT_EQ(gui::SearchResultModel::formatCategory(FileCategory::SourceCode).toStdString(), "Code");
        TEST_ASSERT_EQ(gui::SearchResultModel::formatCategory(FileCategory::Images).toStdString(), "Image");
        TEST_ASSERT_EQ(gui::SearchResultModel::formatCategory(FileCategory::Videos).toStdString(), "Video");
        TEST_ASSERT_EQ(gui::SearchResultModel::formatCategory(FileCategory::Audio).toStdString(), "Audio");
        TEST_ASSERT_EQ(gui::SearchResultModel::formatCategory(FileCategory::Archives).toStdString(), "Archive");
        TEST_ASSERT_EQ(gui::SearchResultModel::formatCategory(FileCategory::Other).toStdString(), "Other");

        TEST_ASSERT_EQ(gui::SearchResultModel::formatTimestamp(0).toStdString(), "Unknown");
        TEST_ASSERT(!gui::SearchResultModel::formatTimestamp(1700000000).isEmpty());
    }

    // Test 3: Populating with SearchResponse
    {
        gui::SearchResultModel model;
        search::SearchResponse resp;
        resp.status = search::SearchStatus::Success;
        resp.query = "algorithm";
        resp.totalMatches = 3;

        search::SearchResultItem item1(1, "dijkstra.cpp", "/src/dijkstra.cpp", "cpp",
                                       FileCategory::SourceCode, 2048, 1600000000, true, 85.5, 1);
        search::SearchResultItem item2(2, "manual.pdf", "/docs/manual.pdf", "pdf",
                                       FileCategory::Documents, 1048576, 1600001000, true, 92.0, 2);
        search::SearchResultItem item3(3, "banner.png", "/assets/banner.png", "png",
                                       FileCategory::Images, 512000, 1600002000, false, 60.0, 3);

        resp.results.push_back(item1);
        resp.results.push_back(item2);
        resp.results.push_back(item3);

        model.setSearchResponse(resp);

        TEST_ASSERT_EQ(model.rowCount(), 3);
        TEST_ASSERT_EQ(model.totalUnfilteredCount(), 3);
        TEST_ASSERT_EQ(model.visibleCount(), 3);

        // Check header data
        TEST_ASSERT_EQ(model.headerData(gui::SearchResultModel::ColFilename, Qt::Horizontal).toString().toStdString(), "File Name");
        TEST_ASSERT_EQ(model.headerData(gui::SearchResultModel::ColScore, Qt::Horizontal).toString().toStdString(), "Relevance");

        // Check item data
        const auto* it = model.getItem(0);
        TEST_ASSERT(it != nullptr);
    }

    // Test 4: Sorting by Score and Name
    {
        gui::SearchResultModel model;
        search::SearchResponse resp;
        resp.results.push_back(search::SearchResultItem(1, "beta.txt", "/beta.txt", "txt", FileCategory::Documents, 100, 100, true, 40.0));
        resp.results.push_back(search::SearchResultItem(2, "alpha.txt", "/alpha.txt", "txt", FileCategory::Documents, 200, 200, true, 90.0));
        resp.results.push_back(search::SearchResultItem(3, "gamma.txt", "/gamma.txt", "txt", FileCategory::Documents, 300, 300, true, 70.0));
        model.setSearchResponse(resp);

        // Sort by Score Descending
        model.sort(gui::SearchResultModel::ColScore, Qt::DescendingOrder);
        TEST_ASSERT_EQ(model.getItem(0)->filename, "alpha.txt");
        TEST_ASSERT_EQ(model.getItem(1)->filename, "gamma.txt");
        TEST_ASSERT_EQ(model.getItem(2)->filename, "beta.txt");

        // Sort by Score Ascending
        model.sort(gui::SearchResultModel::ColScore, Qt::AscendingOrder);
        TEST_ASSERT_EQ(model.getItem(0)->filename, "beta.txt");
        TEST_ASSERT_EQ(model.getItem(1)->filename, "gamma.txt");
        TEST_ASSERT_EQ(model.getItem(2)->filename, "alpha.txt");

        // Sort by Name Ascending
        model.sort(gui::SearchResultModel::ColFilename, Qt::AscendingOrder);
        TEST_ASSERT_EQ(model.getItem(0)->filename, "alpha.txt");
        TEST_ASSERT_EQ(model.getItem(1)->filename, "beta.txt");
        TEST_ASSERT_EQ(model.getItem(2)->filename, "gamma.txt");
    }

    // Test 5: Facet Filtering by Category and Size
    {
        gui::SearchResultModel model;
        search::SearchResponse resp;
        resp.results.push_back(search::SearchResultItem(1, "doc.pdf", "/doc.pdf", "pdf", FileCategory::Documents, 500, 100, true, 50.0));
        resp.results.push_back(search::SearchResultItem(2, "code.py", "/code.py", "py", FileCategory::SourceCode, 5000000, 100, true, 50.0));
        resp.results.push_back(search::SearchResultItem(3, "img.png", "/img.png", "png", FileCategory::Images, 2000, 100, true, 50.0));
        model.setSearchResponse(resp);
        TEST_ASSERT_EQ(model.visibleCount(), 3);

        // Exclude Images
        gui::ResultFilterCriteria crit;
        crit.includeImages = false;
        model.setFilterCriteria(crit);
        TEST_ASSERT_EQ(model.visibleCount(), 2);

        // Filter by size: Max 10,000 bytes
        crit.maxSizeBytes = 10000;
        model.setFilterCriteria(crit);
        TEST_ASSERT_EQ(model.visibleCount(), 1); // Only doc.pdf matches
        TEST_ASSERT_EQ(model.getItem(0)->filename, "doc.pdf");

        // Reset filter
        gui::ResultFilterCriteria resetCrit;
        model.setFilterCriteria(resetCrit);
        TEST_ASSERT_EQ(model.visibleCount(), 3);
    }

    // Test 6: Model Clear
    {
        gui::SearchResultModel model;
        search::SearchResponse resp;
        resp.results.push_back(search::SearchResultItem(1, "a.txt", "/a.txt", "txt", FileCategory::Documents, 10, 10, true, 10.0));
        model.setSearchResponse(resp);
        TEST_ASSERT_EQ(model.rowCount(), 1);

        model.clear();
        TEST_ASSERT_EQ(model.rowCount(), 0);
        TEST_ASSERT_EQ(model.totalUnfilteredCount(), 0);
    }
}

} // namespace sse::test
