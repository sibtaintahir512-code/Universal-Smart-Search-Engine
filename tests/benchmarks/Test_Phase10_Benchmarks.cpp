#include "TestHarness.hpp"
#include "benchmarks/SyntheticCorpusGenerator.hpp"
#include "indexing/FileScanner.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include "indexing/Tokenizer.hpp"
#include "indexing/IndexingPipeline.hpp"
#include "search/SearchService.hpp"
#include "ranking/RankingEngine.hpp"
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

namespace sse::test {

struct MetricSummary {
    double median{0.0};
    double stddev{0.0};
    double minVal{0.0};
    double maxVal{0.0};
    std::size_t runs{0};
};

// Helper to compute median, stddev, min, max over repetitions with min/max discarded
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

    // Median
    std::size_t n = samples.size();
    if (n % 2 == 0) {
        s.median = (samples[n / 2 - 1] + samples[n / 2]) / 2.0;
    } else {
        s.median = samples[n / 2];
    }

    // Mean for stddev
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

    // =========================================================================
    // Benchmark 1: Small Corpus (1,000 files)
    // =========================================================================
    std::cout << "\n[BENCHMARK] Generating 1,000-file Small Synthetic Corpus..." << std::endl;
    auto stats1k = generator.generateCorpus(1000);
    std::cout << "  Generated " << stats1k.totalFiles << " files (" 
              << (stats1k.totalBytes / 1024) << " KB) in " 
              << std::fixed << std::setprecision(1) << stats1k.generationDurationMs << " ms" << std::endl;

    indexing::DocumentCatalog catalog1k;
    indexing::InvertedIndex index1k;
    indexing::Tokenizer tokenizer1k;
    extraction::ExtractionManager extractionMgr1k;
    indexing::IndexingPipeline pipeline1k(catalog1k, index1k, tokenizer1k, &extractionMgr1k);

    indexing::FileScanner scanner1k;
    scanner1k.addRoot(generator.getRootPath().string());
    std::atomic<bool> cancelFlag1k{false};

    // Discovery & Indexing
    auto startIdx = std::chrono::steady_clock::now();
    scanner1k.scan(catalog1k, [&](const indexing::ExtendedFileRecord& rec, FileChangeStatus status) {
        if (status == FileChangeStatus::New) {
            pipeline1k.indexDocument(rec.metadata.docId);
        }
    }, nullptr, cancelFlag1k);
    auto endIdx = std::chrono::steady_clock::now();
    double idxTime1k = std::chrono::duration<double, std::milli>(endIdx - startIdx).count();

    std::cout << "  1k Ingestion Time: " << idxTime1k << " ms (" 
              << (1000.0 / (idxTime1k / 1000.0)) << " docs/sec)" << std::endl;
    std::cout << "  1k Vocabulary: " << index1k.vocabularySize() << " terms | Postings: " 
              << index1k.totalPostingsCount() << std::endl;

    // Search Benchmarks (5 repetitions)
    search::SearchService search1k(&index1k, &catalog1k);

    std::vector<double> singleWordSamples1k;
    std::vector<double> boolQuerySamples1k;
    std::vector<double> bm25Samples1k;

    for (int r = 0; r < 5; ++r) {
        // Warm/Cold single word
        auto t0 = std::chrono::steady_clock::now();
        auto r1 = search1k.search("algorithm", 0, 50);
        auto t1 = std::chrono::steady_clock::now();
        singleWordSamples1k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

        // 3-term boolean query: (dijkstra OR python) AND guide
        t0 = std::chrono::steady_clock::now();
        auto r2 = search1k.search("(dijkstra OR python) AND guide", 0, 50);
        t1 = std::chrono::steady_clock::now();
        boolQuerySamples1k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

        // BM25 ranking mode
        auto cfg = search1k.config();
        cfg.rankingConfig.mode = ranking::RankingMode::BM25;
        search1k.setConfig(cfg);
        t0 = std::chrono::steady_clock::now();
        auto r3 = search1k.search("dijkstra algorithm shortest path", 0, 50);
        t1 = std::chrono::steady_clock::now();
        bm25Samples1k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }

    auto swStats1k = computeStats(singleWordSamples1k);
    auto bqStats1k = computeStats(boolQuerySamples1k);
    auto bmStats1k = computeStats(bm25Samples1k);

    std::cout << "  1k Single-Word Latency (Median): " << swStats1k.median << " ms (StdDev: " << swStats1k.stddev << " ms)" << std::endl;
    std::cout << "  1k 3-Term Boolean Latency (Median): " << bqStats1k.median << " ms (StdDev: " << bqStats1k.stddev << " ms)" << std::endl;
    std::cout << "  1k BM25 Ranking Latency (Median): " << bmStats1k.median << " ms (StdDev: " << bmStats1k.stddev << " ms)" << std::endl;

    // Persistence Benchmark
    std::filesystem::path persistDir = "temp_bench_persist";
    std::filesystem::create_directories(persistDir);
    persistence::IndexPersistenceManager pm1k(&index1k, &catalog1k, persistDir);

    std::vector<double> saveSamples1k;
    std::vector<double> loadSamples1k;
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
    auto saveStats1k = computeStats(saveSamples1k);
    auto loadStats1k = computeStats(loadSamples1k);
    std::cout << "  1k Save Latency (Median): " << saveStats1k.median << " ms | Load Latency: " << loadStats1k.median << " ms" << std::endl;

    // Cleanup 1k
    generator.cleanup();

    // =========================================================================
    // Benchmark 2: Medium Corpus (10,000 files)
    // =========================================================================
    std::cout << "\n[BENCHMARK] Generating 10,000-file Medium Synthetic Corpus..." << std::endl;
    auto stats10k = generator.generateCorpus(10000);
    std::cout << "  Generated " << stats10k.totalFiles << " files (" 
              << (stats10k.totalBytes / 1024) << " KB) in " 
              << std::fixed << std::setprecision(1) << stats10k.generationDurationMs << " ms" << std::endl;

    indexing::DocumentCatalog catalog10k;
    indexing::InvertedIndex index10k;
    indexing::Tokenizer tokenizer10k;
    extraction::ExtractionManager extractionMgr10k;
    indexing::IndexingPipeline pipeline10k(catalog10k, index10k, tokenizer10k, &extractionMgr10k);

    indexing::FileScanner scanner10k;
    scanner10k.addRoot(generator.getRootPath().string());
    std::atomic<bool> cancelFlag10k{false};

    startIdx = std::chrono::steady_clock::now();
    scanner10k.scan(catalog10k, [&](const indexing::ExtendedFileRecord& rec, FileChangeStatus status) {
        if (status == FileChangeStatus::New) {
            pipeline10k.indexDocument(rec.metadata.docId);
        }
    }, nullptr, cancelFlag10k);
    endIdx = std::chrono::steady_clock::now();
    double idxTime10k = std::chrono::duration<double, std::milli>(endIdx - startIdx).count();

    std::cout << "  10k Ingestion Time: " << idxTime10k << " ms (" 
              << (10000.0 / (idxTime10k / 1000.0)) << " docs/sec)" << std::endl;
    std::cout << "  10k Vocabulary: " << index10k.vocabularySize() << " terms | Postings: " 
              << index10k.totalPostingsCount() << std::endl;

    search::SearchService search10k(&index10k, &catalog10k);
    std::vector<double> singleWordSamples10k;
    std::vector<double> boolQuerySamples10k;
    std::vector<double> bm25Samples10k;

    for (int r = 0; r < 5; ++r) {
        auto t0 = std::chrono::steady_clock::now();
        auto r1 = search10k.search("algorithm", 0, 50);
        auto t1 = std::chrono::steady_clock::now();
        singleWordSamples10k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

        t0 = std::chrono::steady_clock::now();
        auto r2 = search10k.search("(dijkstra OR python) AND guide", 0, 50);
        t1 = std::chrono::steady_clock::now();
        boolQuerySamples10k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());

        auto cfg = search10k.config();
        cfg.rankingConfig.mode = ranking::RankingMode::BM25;
        search10k.setConfig(cfg);
        t0 = std::chrono::steady_clock::now();
        auto r3 = search10k.search("dijkstra algorithm shortest path", 0, 50);
        t1 = std::chrono::steady_clock::now();
        bm25Samples10k.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }

    auto swStats10k = computeStats(singleWordSamples10k);
    auto bqStats10k = computeStats(boolQuerySamples10k);
    auto bmStats10k = computeStats(bm25Samples10k);

    std::cout << "  10k Single-Word Latency (Median): " << swStats10k.median << " ms (StdDev: " << swStats10k.stddev << " ms)" << std::endl;
    std::cout << "  10k 3-Term Boolean Latency (Median): " << bqStats10k.median << " ms (StdDev: " << bqStats10k.stddev << " ms)" << std::endl;
    std::cout << "  10k BM25 Ranking Latency (Median): " << bmStats10k.median << " ms (StdDev: " << bmStats10k.stddev << " ms)" << std::endl;

    // Cleanup 10k
    generator.cleanup();

    // =========================================================================
    // Benchmark 3: 100,000 Corpus Staging & Verification
    // =========================================================================
    std::cout << "\n[BENCHMARK] Executing 100,000-Document Inverted Index & Trie Stress Simulation..." << std::endl;
    indexing::InvertedIndex index100k;
    indexing::DocumentCatalog catalog100k;

    auto tStart100k = std::chrono::steady_clock::now();
    // Ingest 100,000 simulated document postings
    for (DocID id = 1; id <= 100000; ++id) {
        index100k.addTerm("dijkstra", id, 1, 3, FIELD_FILENAME | FIELD_CONTENT);
        if (id % 2 == 0) index100k.addTerm("algorithm", id, 0, 2, FIELD_CONTENT);
        if (id % 5 == 0) index100k.addTerm("python", id, 1, 1, FIELD_FILENAME | FIELD_CONTENT);
        if (id % 10 == 0) index100k.addTerm("guide", id, 0, 4, FIELD_CONTENT);
    }
    auto tEnd100k = std::chrono::steady_clock::now();
    double stageTime100k = std::chrono::duration<double, std::milli>(tEnd100k - tStart100k).count();

    std::cout << "  100k Posting Construction Time: " << stageTime100k << " ms" << std::endl;
    std::cout << "  100k Inverted Index Postings Count: " << index100k.totalPostingsCount() << std::endl;

    // Measure Trie Autocomplete on 100k vocabulary space
    std::vector<double> trieSamples;
    for (int r = 0; r < 5; ++r) {
        auto t0 = std::chrono::steady_clock::now();
        auto suggestions = index100k.trie().getTopKSuggestions("dijk", 5);
        auto t1 = std::chrono::steady_clock::now();
        trieSamples.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    auto trieStats = computeStats(trieSamples);
    std::cout << "  Trie Autocomplete Latency (Median): " << trieStats.median << " ms (StdDev: " << trieStats.stddev << " ms)" << std::endl;

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
