#include "TestHarness.hpp"
#include "benchmarks/SyntheticCorpusGenerator.hpp"
#include "indexing/FileScanner.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include "indexing/Tokenizer.hpp"
#include "indexing/IndexingPipeline.hpp"
#include "search/SearchService.hpp"
#include "ranking/RankingEngine.hpp"
#include "ranking/BM25Scorer.hpp"
#include "ranking/TFIDFScorer.hpp"
#include "ranking/TopKSelector.hpp"
#include "persistence/IndexPersistenceManager.hpp"
#include "persistence/IndexCompactor.hpp"
#include "persistence/CRC32.hpp"
#include "gui/models/SearchResultModel.hpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <filesystem>

namespace sse::test {

struct MetricSummary {
    double median{0.0};
    double stddev{0.0};
    double minVal{0.0};
    double maxVal{0.0};
    std::size_t runs{0};
};

// Helper to compute median, stddev, min, max over 5 repetitions with min/max discarded
static MetricSummary computeStats(std::vector<double> samples) {
    MetricSummary s;
    s.runs = samples.size();
    if (samples.empty()) return s;

    std::sort(samples.begin(), samples.end());
    s.minVal = samples.front();
    s.maxVal = samples.back();

    if (samples.size() >= 5) {
        // Discard min and max outliers
        samples.erase(samples.begin());
        samples.pop_back();
    }

    std::size_t n = samples.size();
    if (n % 2 == 0) {
        s.median = (samples[n / 2 - 1] + samples[n / 2]) / 2.0;
    } else {
        s.median = samples[n / 2];
    }

    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    double mean = sum / n;
    double sqDiffSum = 0.0;
    for (double v : samples) {
        sqDiffSum += (v - mean) * (v - mean);
    }
    s.stddev = std::sqrt(sqDiffSum / n);

    return s;
}

void runPhase10Benchmarks() {
    std::cout << "\n======================================================================" << std::endl;
    std::cout << " EXECUTING PHASE 10 SYSTEM BENCHMARK SUITE (1k / 10k / 100k Staged)" << std::endl;
    std::cout << "======================================================================" << std::endl;

    benchmark::SyntheticCorpusGenerator generator("benchmark_temp_corpus", 12345);
    std::filesystem::path persistDir = "temp_bench_persist";
    std::filesystem::create_directories(persistDir);

    // =========================================================================
    // Benchmark 1: Small Corpus (1,000 files)
    // =========================================================================
    std::cout << "\n[BENCHMARK 1] Evaluating 1,000-File Small Corpus..." << std::endl;
    auto stats1k = generator.generateCorpus(1000);

    indexing::DocumentCatalog catalog1k;
    indexing::InvertedIndex index1k;
    indexing::Tokenizer tokenizer1k;
    extraction::ExtractionManager extractionMgr1k;
    indexing::IndexingPipeline pipeline1k(catalog1k, index1k, tokenizer1k, &extractionMgr1k);

    indexing::FileScanner scanner1k;
    scanner1k.addRoot(generator.getRootPath().string());
    std::atomic<bool> cancelFlag1k{false};

    auto startIdx1k = std::chrono::steady_clock::now();
    scanner1k.scan(catalog1k, [&](const indexing::ExtendedFileRecord& rec, FileChangeStatus status) {
        if (status == FileChangeStatus::New) {
            pipeline1k.indexDocument(rec.metadata.docId);
        }
    }, nullptr, cancelFlag1k);
    auto endIdx1k = std::chrono::steady_clock::now();
    double idxTime1k = std::chrono::duration<double, std::milli>(endIdx1k - startIdx1k).count();

    std::cout << "  1k Ingestion Time: " << idxTime1k << " ms (" 
              << (1000.0 / (idxTime1k / 1000.0)) << " docs/sec)" << std::endl;
    std::cout << "  1k Vocabulary: " << index1k.vocabularySize() << " terms | Postings: " 
              << index1k.totalPostingsCount() << std::endl;

    search::SearchConfig searchCfg1k;
    searchCfg1k.rankingConfig.mode = ranking::RankingMode::BM25;
    search::SearchService search1k(&index1k, &catalog1k, searchCfg1k);

    std::vector<double> swSamples1k, andSamples1k, bqSamples1k, orSamples1k, notSamples1k, bmSamples1k, tfidfSamples1k;
    for (int r = 0; r < 5; ++r) {
        auto t0 = std::chrono::steady_clock::now();
        search1k.search("algorithm", 0, 50);
        auto t1 = std::chrono::steady_clock::now();
        swSamples1k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

        t0 = std::chrono::steady_clock::now();
        search1k.search("dijkstra AND algorithm", 0, 50);
        t1 = std::chrono::steady_clock::now();
        andSamples1k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

        t0 = std::chrono::steady_clock::now();
        search1k.search("(dijkstra OR python) AND guide", 0, 50);
        t1 = std::chrono::steady_clock::now();
        bqSamples1k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

        t0 = std::chrono::steady_clock::now();
        search1k.search("dijkstra OR python", 0, 50);
        t1 = std::chrono::steady_clock::now();
        orSamples1k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

        t0 = std::chrono::steady_clock::now();
        search1k.search("dijkstra AND NOT python", 0, 50);
        t1 = std::chrono::steady_clock::now();
        notSamples1k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

        t0 = std::chrono::steady_clock::now();
        search1k.search("dijkstra algorithm shortest path", 0, 50);
        t1 = std::chrono::steady_clock::now();
        bmSamples1k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }

    auto swStats1k = computeStats(swSamples1k);
    auto andStats1k = computeStats(andSamples1k);
    auto bqStats1k = computeStats(bqSamples1k);
    auto bmStats1k = computeStats(bmSamples1k);

    std::cout << "  1k Single-Word (Median): " << swStats1k.median << " ms" << std::endl;
    std::cout << "  1k 2-Term AND (Median): " << andStats1k.median << " ms" << std::endl;
    std::cout << "  1k 3-Term Boolean (Median): " << bqStats1k.median << " ms" << std::endl;
    std::cout << "  1k BM25 Top-50 (Median): " << bmStats1k.median << " ms" << std::endl;

    // Persistence 1k
    persistence::IndexPersistenceManager pm1k(&index1k, &catalog1k, persistDir);
    std::vector<double> saveSamples1k, loadSamples1k;
    for (int r = 0; r < 5; ++r) {
        auto t0 = std::chrono::steady_clock::now();
        pm1k.saveIndex("bench_1k.idx");
        auto t1 = std::chrono::steady_clock::now();
        saveSamples1k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

        t0 = std::chrono::steady_clock::now();
        pm1k.loadIndex("bench_1k.idx");
        t1 = std::chrono::steady_clock::now();
        loadSamples1k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    std::cout << "  1k Save (Median): " << computeStats(saveSamples1k).median << " ms | Load: " 
              << computeStats(loadSamples1k).median << " ms" << std::endl;

    generator.cleanup();

    // =========================================================================
    // Benchmark 2: Medium Corpus (10,000 files)
    // =========================================================================
    std::cout << "\n[BENCHMARK 2] Evaluating 10,000-File Medium Corpus..." << std::endl;
    auto stats10k = generator.generateCorpus(10000);

    indexing::DocumentCatalog catalog10k;
    indexing::InvertedIndex index10k;
    indexing::Tokenizer tokenizer10k;
    extraction::ExtractionManager extractionMgr10k;
    indexing::IndexingPipeline pipeline10k(catalog10k, index10k, tokenizer10k, &extractionMgr10k);

    indexing::FileScanner scanner10k;
    scanner10k.addRoot(generator.getRootPath().string());
    std::atomic<bool> cancelFlag10k{false};

    auto startIdx10k = std::chrono::steady_clock::now();
    scanner10k.scan(catalog10k, [&](const indexing::ExtendedFileRecord& rec, FileChangeStatus status) {
        if (status == FileChangeStatus::New) {
            pipeline10k.indexDocument(rec.metadata.docId);
        }
    }, nullptr, cancelFlag10k);
    auto endIdx10k = std::chrono::steady_clock::now();
    double idxTime10k = std::chrono::duration<double, std::milli>(endIdx10k - startIdx10k).count();

    std::cout << "  10k Ingestion Time: " << idxTime10k << " ms (" 
              << (10000.0 / (idxTime10k / 1000.0)) << " docs/sec)" << std::endl;
    std::cout << "  10k Vocabulary: " << index10k.vocabularySize() << " terms | Postings: " 
              << index10k.totalPostingsCount() << std::endl;

    search::SearchConfig searchCfg10k;
    searchCfg10k.rankingConfig.mode = ranking::RankingMode::BM25;
    search::SearchService search10k(&index10k, &catalog10k, searchCfg10k);

    std::vector<double> swSamples10k, andSamples10k, bqSamples10k, orSamples10k, notSamples10k, bmSamples10k;
    for (int r = 0; r < 5; ++r) {
        auto t0 = std::chrono::steady_clock::now();
        search10k.search("algorithm", 0, 50);
        auto t1 = std::chrono::steady_clock::now();
        swSamples10k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

        t0 = std::chrono::steady_clock::now();
        search10k.search("dijkstra AND algorithm", 0, 50);
        t1 = std::chrono::steady_clock::now();
        andSamples10k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

        t0 = std::chrono::steady_clock::now();
        search10k.search("(dijkstra OR python) AND guide", 0, 50);
        t1 = std::chrono::steady_clock::now();
        bqSamples10k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

        t0 = std::chrono::steady_clock::now();
        search10k.search("dijkstra algorithm shortest path", 0, 50);
        t1 = std::chrono::steady_clock::now();
        bmSamples10k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }

    auto swStats10k = computeStats(swSamples10k);
    auto andStats10k = computeStats(andSamples10k);
    auto bqStats10k = computeStats(bqSamples10k);
    auto bmStats10k = computeStats(bmSamples10k);

    std::cout << "  10k Single-Word (Median): " << swStats10k.median << " ms (StdDev: " << swStats10k.stddev << " ms)" << std::endl;
    std::cout << "  10k 2-Term AND (Median): " << andStats10k.median << " ms (StdDev: " << andStats10k.stddev << " ms)" << std::endl;
    std::cout << "  10k 3-Term Boolean (Median): " << bqStats10k.median << " ms (StdDev: " << bqStats10k.stddev << " ms)" << std::endl;
    std::cout << "  10k BM25 Ranking (Median): " << bmStats10k.median << " ms (StdDev: " << bmStats10k.stddev << " ms)" << std::endl;

    // Persistence 10k
    persistence::IndexPersistenceManager pm10k(&index10k, &catalog10k, persistDir);
    std::vector<double> saveSamples10k, loadSamples10k;
    for (int r = 0; r < 5; ++r) {
        auto t0 = std::chrono::steady_clock::now();
        pm10k.saveIndex("bench_10k.idx");
        auto t1 = std::chrono::steady_clock::now();
        saveSamples10k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

        t0 = std::chrono::steady_clock::now();
        pm10k.loadIndex("bench_10k.idx");
        t1 = std::chrono::steady_clock::now();
        loadSamples10k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    std::cout << "  10k Save (Median): " << computeStats(saveSamples10k).median << " ms | Load: " 
              << computeStats(loadSamples10k).median << " ms" << std::endl;

    generator.cleanup();

    // =========================================================================
    // Benchmark 3: 100,000-Document Corpus with ~50,000 Unique Terms
    // =========================================================================
    std::cout << "\n[BENCHMARK 3] Evaluating 100,000-Document Corpus (~50k Vocabulary)..." << std::endl;
    indexing::InvertedIndex index100k;
    indexing::DocumentCatalog catalog100k;

    auto tStart100k = std::chrono::steady_clock::now();
    // 1. Register 100,000 documents with 60% code, 25% media, 15% other distribution
    for (DocID id = 1; id <= 100000; ++id) {
        FileMetadata meta;
        if (id <= 60000) meta.category = FileCategory::SourceCode;
        else if (id <= 85000) meta.category = FileCategory::Images;
        else meta.category = FileCategory::Documents;
        meta.flags = META_FLAG_ACTIVE;
        meta.fileSize = 1024 + (id % 4096);
        catalog100k.registerDocument(meta, "/corpus/doc_" + std::to_string(id) + ".txt", "doc_" + std::to_string(id) + ".txt");
    }

    // 2. Ingest 50,000 unique vocabulary terms into Trie and Inverted Index
    const uint32_t UNIQUE_TERMS = 50000;
    for (uint32_t t = 1; t <= UNIQUE_TERMS; ++t) {
        std::string term = "term" + std::to_string(t);
        DocID docId = (t * 2) % 100000 + 1;
        index100k.addTerm(term, docId, 1, 3, FIELD_FILENAME | FIELD_CONTENT);
        if (t % 5 == 0) {
            index100k.addTerm(term, (docId + 7) % 100000 + 1, 0, 2, FIELD_CONTENT);
        }
    }
    // Anchor terms for multi-term queries
    for (DocID id = 1; id <= 10000; ++id) {
        index100k.addTerm("dijkstra", id, 1, 2, FIELD_FILENAME | FIELD_CONTENT);
        if (id % 2 == 0) index100k.addTerm("algorithm", id, 0, 3, FIELD_CONTENT);
        if (id % 4 == 0) index100k.addTerm("shortest", id, 0, 1, FIELD_CONTENT);
    }

    auto tEnd100k = std::chrono::steady_clock::now();
    double stageTime100k = std::chrono::duration<double, std::milli>(tEnd100k - tStart100k).count();

    std::cout << "  100k Construction Time: " << stageTime100k << " ms" << std::endl;
    std::cout << "  100k Catalog Docs: " << catalog100k.activeCount() 
              << " | Vocabulary: " << index100k.vocabularySize() 
              << " | Postings: " << index100k.totalPostingsCount() << std::endl;

    search::SearchConfig searchCfg100k;
    searchCfg100k.rankingConfig.mode = ranking::RankingMode::BM25;
    search::SearchService search100k(&index100k, &catalog100k, searchCfg100k);

    // 100k Query Benchmarks (5 repetitions)
    std::vector<double> swSamples100k, andSamples100k, bqSamples100k, trieSamples100k;
    for (int r = 0; r < 5; ++r) {
        auto t0 = std::chrono::steady_clock::now();
        search100k.search("term12345", 0, 50);
        auto t1 = std::chrono::steady_clock::now();
        swSamples100k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

        t0 = std::chrono::steady_clock::now();
        search100k.search("dijkstra AND algorithm", 0, 50);
        t1 = std::chrono::steady_clock::now();
        andSamples100k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

        t0 = std::chrono::steady_clock::now();
        search100k.search("(dijkstra OR algorithm) AND shortest", 0, 50);
        t1 = std::chrono::steady_clock::now();
        bqSamples100k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

        t0 = std::chrono::steady_clock::now();
        index100k.trie().getTopKSuggestions("term12", 5);
        t1 = std::chrono::steady_clock::now();
        trieSamples100k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }

    auto swStats100k = computeStats(swSamples100k);
    auto andStats100k = computeStats(andSamples100k);
    auto bqStats100k = computeStats(bqSamples100k);
    auto trieStats100k = computeStats(trieSamples100k);

    std::cout << "  100k Single-Word (Median): " << swStats100k.median << " ms (Target: <= 5.0 ms)" << std::endl;
    std::cout << "  100k 2-Term AND (Median): " << andStats100k.median << " ms" << std::endl;
    std::cout << "  100k 3-Term Boolean (Median): " << bqStats100k.median << " ms (Target: <= 50.0 ms)" << std::endl;
    std::cout << "  100k Trie Autocomplete (Median): " << trieStats100k.median << " ms (Target: <= 5.0 ms)" << std::endl;

    // 100k Deserialization Latency
    persistence::IndexPersistenceManager pm100k(&index100k, &catalog100k, persistDir);
    pm100k.saveIndex("bench_100k.idx");

    std::vector<double> loadSamples100k;
    for (int r = 0; r < 5; ++r) {
        indexing::InvertedIndex freshIdx;
        indexing::DocumentCatalog freshCat;
        persistence::IndexPersistenceManager freshPm(&freshIdx, &freshCat, persistDir);
        auto t0 = std::chrono::steady_clock::now();
        freshPm.loadIndex("bench_100k.idx");
        auto t1 = std::chrono::steady_clock::now();
        loadSamples100k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    auto loadStats100k = computeStats(loadSamples100k);
    std::cout << "  100k Deserialization (Median): " << loadStats100k.median << " ms (" 
              << (loadStats100k.median / 1000.0) << " s) (Target: <= 3.0 s)" << std::endl;

    // Measure CRC-32 on 10 MB payload
    std::vector<uint8_t> crcBuf(10ULL * 1024 * 1024, 0xAB);
    auto t0Crc = std::chrono::steady_clock::now();
    uint32_t cVal = persistence::CRC32::calculate(crcBuf.data(), crcBuf.size());
    auto t1Crc = std::chrono::steady_clock::now();
    double crcTime = std::chrono::duration<double, std::milli>(t1Crc - t0Crc).count();
    double crcThroughput = 10.0 / (crcTime / 1000.0);
    std::cout << "  CRC-32 Throughput: " << crcThroughput << " MB/s (10 MB in " << crcTime << " ms, CRC=0x" << std::hex << cVal << std::dec << ")" << std::endl;

    std::filesystem::remove_all(persistDir);
    std::cout << "======================================================================" << std::endl;
}

} // namespace sse::test
