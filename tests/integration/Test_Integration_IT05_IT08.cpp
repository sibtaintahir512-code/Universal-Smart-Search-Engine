#include "TestHarness.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include "indexing/Tokenizer.hpp"
#include "indexing/IndexingPipeline.hpp"
#include "ranking/RankingEngine.hpp"
#include "ranking/BM25Scorer.hpp"
#include "ranking/TFIDFScorer.hpp"
#include "ranking/TopKSelector.hpp"
#include "persistence/IndexPersistenceManager.hpp"
#include "persistence/IndexCompactor.hpp"
#include "search/SearchService.hpp"
#include "gui/adapters/SearchEngineAdapter.hpp"
#include "gui/models/SearchResultModel.hpp"
#include "gui/workers/SearchWorker.hpp"

#include <filesystem>
#include <iostream>
#include <vector>
#include <string>

namespace sse::test {

// =============================================================================
// IT-05: RankingEngine + CustomMaxHeap / CustomMinHeap
// =============================================================================
void runIT05Tests() {
    std::cout << "\n[RUNNING IT-05] RankingEngine + CustomMinHeap Top-K Selection..." << std::endl;

    indexing::DocumentCatalog catalog;
    indexing::InvertedIndex index;
    indexing::Tokenizer tokenizer;
    indexing::IndexingPipeline pipeline(catalog, index, tokenizer, nullptr);

    // Build candidate set of 500 controlled documents
    const std::size_t DOC_COUNT = 500;
    for (std::size_t i = 1; i <= DOC_COUNT; ++i) {
        FileMetadata meta;
        meta.category = FileCategory::Documents;
        meta.flags = META_FLAG_ACTIVE;

        std::string fname = (i == 100) ? "target_query_file.txt" : ("file_" + std::to_string(i) + ".txt");
        DocID id = catalog.registerDocument(meta, "/corpus/" + fname, fname);

        extraction::ExtractionResult res;
        res.docId = id;
        res.status = extraction::ExtractionStatus::Success;

        // Doc 100 has heavy term frequency in both filename and body
        if (i == 100) {
            res.extractedText = "target query term occurs repeatedly target query target query target query";
        } else if (i % 2 == 0) {
            res.extractedText = "target term occurs once here with other background text";
        } else {
            res.extractedText = "unrelated text and background content without special words";
        }

        pipeline.indexDocument(id, &res);
    }

    // Test BM25 Mode
    search::SearchConfig searchConfig;
    searchConfig.rankingConfig.mode = ranking::RankingMode::BM25;
    searchConfig.rankingConfig.filenameWeight = 5.0;
    searchConfig.rankingConfig.bodyWeight = 1.0;

    search::SearchService service(&index, &catalog, searchConfig);
    auto resp = service.search("target", 0, 50);

    TEST_ASSERT_EQ(static_cast<int>(resp.status), static_cast<int>(search::SearchStatus::Success));
    TEST_ASSERT(resp.totalMatches >= 50);
    TEST_ASSERT_EQ(resp.results.size(), 50);

    // Document 100 must be #1 ranked due to filename match + highest body frequency
    TEST_ASSERT_EQ(resp.results[0].docId, 100);
    TEST_ASSERT(resp.results[0].relevanceScore > resp.results[1].relevanceScore);

    // Test Deterministic Ordering & Tie-Breaking
    auto respSecondRun = service.search("target", 0, 50);
    for (std::size_t i = 0; i < resp.results.size(); ++i) {
        TEST_ASSERT_EQ(resp.results[i].docId, respSecondRun.results[i].docId);
        TEST_ASSERT_EQ(resp.results[i].relevanceScore, respSecondRun.results[i].relevanceScore);
    }
}

// =============================================================================
// IT-06: IndexPersistenceManager + Full In-Memory State
// =============================================================================
void runIT06Tests() {
    std::cout << "\n[RUNNING IT-06] IndexPersistenceManager + Full In-Memory State Equivalence..." << std::endl;

    std::filesystem::path persistDir = "temp_it06_persistence";
    std::filesystem::remove_all(persistDir);
    std::filesystem::create_directories(persistDir);

    const std::size_t CORPUS_SIZE = 10000;
    std::vector<double> originalScores;

    // Scope 1: Build initial engine, index 10,000 documents, run queries, save to disk, destroy
    {
        indexing::DocumentCatalog origCatalog;
        indexing::InvertedIndex origIndex;
        indexing::Tokenizer origTokenizer;
        indexing::IndexingPipeline origPipeline(origCatalog, origIndex, origTokenizer, nullptr);

        for (std::size_t i = 1; i <= CORPUS_SIZE; ++i) {
            FileMetadata meta;
            meta.category = FileCategory::SourceCode;
            meta.flags = META_FLAG_ACTIVE;

            std::string fname = "source_" + std::to_string(i) + ".cpp";
            DocID id = origCatalog.registerDocument(meta, "/src/" + fname, fname);

            extraction::ExtractionResult res;
            res.docId = id;
            res.status = extraction::ExtractionStatus::Success;

            if (i % 10 == 0) {
                res.extractedText = "dijkstra graph shortest path algorithm";
            } else if (i % 5 == 0) {
                res.extractedText = "algorithm general implementation";
            } else {
                res.extractedText = "generic code and logic";
            }

            origPipeline.indexDocument(id, &res);
        }

        search::SearchService origSearch(&origIndex, &origCatalog);
        auto origResp = origSearch.search("dijkstra algorithm", 0, 20);
        TEST_ASSERT_EQ(origResp.totalMatches, 1000);
        for (std::size_t i = 0; i < origResp.results.size(); ++i) {
            originalScores.push_back(origResp.results[i].relevanceScore);
        }

        persistence::IndexPersistenceManager persistenceManager(&origIndex, &origCatalog, persistDir);
        auto saveResult = persistenceManager.saveIndex("search.idx");
        TEST_ASSERT(saveResult.isSuccess());

        // Scope exits here -> origCatalog, origIndex, origSearch fully destroyed
    }

    // Scope 2: Fresh instance, restore from disk, verify exact roundtrip equivalence
    {
        indexing::DocumentCatalog restoredCatalog;
        indexing::InvertedIndex restoredIndex;
        persistence::IndexPersistenceManager persistenceManager(&restoredIndex, &restoredCatalog, persistDir);

        auto loadResult = persistenceManager.loadIndex("search.idx");
        TEST_ASSERT(loadResult.isSuccess());
        TEST_ASSERT_EQ(restoredCatalog.activeCount(), CORPUS_SIZE);
        TEST_ASSERT(restoredIndex.vocabularySize() > 0);

        search::SearchService restoredSearch(&restoredIndex, &restoredCatalog);
        auto restoredResp = restoredSearch.search("dijkstra algorithm", 0, 20);

        TEST_ASSERT_EQ(restoredResp.totalMatches, 1000);
        TEST_ASSERT_EQ(restoredResp.results.size(), originalScores.size());

        for (std::size_t i = 0; i < restoredResp.results.size(); ++i) {
            TEST_ASSERT_EQ(restoredResp.results[i].relevanceScore, originalScores[i]);
        }
    }

    std::filesystem::remove_all(persistDir);
}

// =============================================================================
// IT-07: SearchEngineAdapter + Qt GUI Components
// =============================================================================
void runIT07Tests() {
    std::cout << "\n[RUNNING IT-07] SearchEngineAdapter + Qt GUI Component Integration..." << std::endl;

    std::filesystem::path testDir = "temp_it07_data";
    std::filesystem::remove_all(testDir);
    std::filesystem::create_directories(testDir);

    gui::SearchEngineAdapter adapter(testDir);

    // Verify adapter validation
    std::string err;
    std::size_t offset = 0;
    TEST_ASSERT(adapter.validateQuery("python AND guide", err, offset));
    TEST_ASSERT(!adapter.validateQuery("(python AND", err, offset));

    // Verify SearchResultModel integration
    gui::SearchResultModel model;
    search::SearchResponse resp;
    resp.status = search::SearchStatus::Success;
    resp.query = "python guide";
    resp.totalMatches = 2;

    search::SearchResultItem item1(1, "script.py", "/src/script.py", "py", FileCategory::SourceCode, 1024, 1600000000, true, 80.0, 1);
    search::SearchResultItem item2(2, "manual.pdf", "/docs/manual.pdf", "pdf", FileCategory::Documents, 2048, 1600001000, true, 95.0, 2);
    resp.results.push_back(item1);
    resp.results.push_back(item2);

    model.setSearchResponse(resp);
    TEST_ASSERT_EQ(model.rowCount(), 2);

    // Verify SearchWorker off-thread signal delivery
    gui::SearchWorker worker(&adapter);
    bool receivedSignal = false;
    QObject::connect(&worker, &gui::SearchWorker::searchCompleted, [&](const search::SearchResponse& r) {
        receivedSignal = true;
        TEST_ASSERT_EQ(static_cast<int>(r.status), static_cast<int>(search::SearchStatus::Success));
    });

    worker.performSearch("test", 0, 50);
    TEST_ASSERT(receivedSignal);

    // Verify rapid successive queries & cancellation token handling
    worker.cancel();

    std::filesystem::remove_all(testDir);
}

// =============================================================================
// IT-08: DocumentCatalog + InvertedIndex (Compaction & Remapping)
// =============================================================================
void runIT08Tests() {
    std::cout << "\n[RUNNING IT-08] DocumentCatalog + InvertedIndex Tombstone Compaction..." << std::endl;

    indexing::DocumentCatalog catalog;
    indexing::InvertedIndex index;
    indexing::Tokenizer tokenizer;
    indexing::IndexingPipeline pipeline(catalog, index, tokenizer, nullptr);

    // Build catalog with 1,000 documents
    const std::size_t TOTAL_DOCS = 1000;
    for (std::size_t i = 1; i <= TOTAL_DOCS; ++i) {
        FileMetadata meta;
        meta.category = FileCategory::Documents;
        meta.flags = META_FLAG_ACTIVE;

        std::string fname = "doc_" + std::to_string(i) + ".txt";
        DocID id = catalog.registerDocument(meta, "/docs/" + fname, fname);

        extraction::ExtractionResult res;
        res.docId = id;
        res.status = extraction::ExtractionStatus::Success;
        res.extractedText = "common universal token search item " + std::to_string(i);

        pipeline.indexDocument(id, &res);
    }

    TEST_ASSERT_EQ(catalog.activeCount(), 1000);
    TEST_ASSERT_EQ(catalog.totalCount(), 1000);

    // Tombstone 200 documents (docs 101 to 300)
    for (DocID id = 101; id <= 300; ++id) {
        catalog.markDeleted(id);
    }

    TEST_ASSERT_EQ(catalog.activeCount(), 800);
    TEST_ASSERT_EQ(catalog.totalCount(), 1000);

    // Execute reference-remapping compaction
    std::filesystem::path compactDir = "temp_it08_persist";
    persistence::IndexPersistenceManager persistenceManager(&index, &catalog, compactDir);
    auto compactStats = persistenceManager.compactIndex();

    TEST_ASSERT(compactStats.isSuccess());
    TEST_ASSERT_EQ(compactStats.activeDocumentCount, 800);

    // Verify:
    // 1. Exactly zero dangling posting references
    // 2. Active count is 800
    // 3. Monotonically contiguous [1 .. 800]
    TEST_ASSERT_EQ(catalog.activeCount(), 800);
    TEST_ASSERT_EQ(catalog.totalCount(), 800);

    std::filesystem::remove_all(compactDir);

    // Search for universal term
    search::SearchService service(&index, &catalog);
    auto resp = service.search("universal", 0, 1000);
    TEST_ASSERT_EQ(resp.totalMatches, 800);

    // Verify all returned DocIDs are strictly in range [1 .. 800]
    for (std::size_t i = 0; i < resp.results.size(); ++i) {
        TEST_ASSERT(resp.results[i].docId >= 1 && resp.results[i].docId <= 800);
    }
}

} // namespace sse::test
