#include "TestHarness.hpp"
#include "indexing/IndexingPipeline.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include "indexing/IndexIntegrityChecker.hpp"
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace sse::test {

void runPhase5BenchmarkTests() {
    std::cout << "\n[BENCHMARK] Executing Phase 5 Inverted Index & Tokenizer Performance Benchmarks..." << std::endl;

    using namespace sse::indexing;
    using namespace sse::extraction;
    using sse::extraction::ExtractionStatus;

    // Fixed vocabulary seed for reproducible synthetic document generation
    const std::vector<std::string> vocab = {
        "search", "engine", "algorithm", "data", "structure", "indexing",
        "hash", "table", "trie", "heap", "priority", "queue", "stack",
        "posting", "list", "document", "catalog", "token", "normalization",
        "boolean", "ranking", "tfidf", "memory", "performance", "systems",
        "computer", "science", "software", "engineering", "optimization",
        "filesystem", "scanner", "traversal", "breadth", "first", "binary",
        "serialization", "persistence", "cache", "unicode", "utf8", "query"
    };

    auto generateDocText = [&](std::size_t docIndex, std::size_t wordCount) -> std::string {
        std::string text;
        text.reserve(wordCount * 10);
        for (std::size_t w = 0; w < wordCount; ++w) {
            std::size_t wordIdx = (docIndex * 17 + w * 31) % vocab.size();
            text += vocab[wordIdx];
            text.push_back(' ');
        }
        return text;
    };

    auto runBenchmarkScale = [&](std::size_t N) {
        DocumentCatalog catalog;
        InvertedIndex index;
        IndexingPipeline pipeline(catalog, index);

        auto t0 = std::chrono::steady_clock::now();

        for (std::size_t i = 1; i <= N; ++i) {
            FileMetadata meta;
            meta.fileSize = 1000 + (i % 500);
            meta.category = FileCategory::Documents;

            std::string filename = "file_" + std::to_string(i) + ".txt";
            std::string path = "/corpus/" + filename;
            DocID docId = catalog.registerDocument(meta, path, filename);

            ExtractionResult ext;
            ext.docId = docId;
            ext.sourcePath = path;

            if (i % 20 == 0) {
                // Every 20th document is MetadataOnly
                ext.status = ExtractionStatus::MetadataOnly;
            } else if (i % 15 == 0) {
                // Every 15th document is Partial
                ext.status = ExtractionStatus::Partial;
                ext.isTruncated = true;
                ext.extractedText = generateDocText(i, 30);
            } else {
                // Full text document (~80 words)
                ext.status = ExtractionStatus::Success;
                ext.extractedText = generateDocText(i, 80);
            }

            IndexingResult res = pipeline.indexDocument(docId, &ext);
            TEST_ASSERT(res.success);
        }

        auto t1 = std::chrono::steady_clock::now();
        double totalMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

        // Integrity audit after full batch ingestion
        IntegrityReport rep = IndexIntegrityChecker::checkIntegrity(index, &catalog);
        TEST_ASSERT(rep.isValid);
        TEST_ASSERT_EQ(rep.errorCount, 0ULL);

        const auto& stats = pipeline.statistics();
        double docsPerSec = (N * 1000.0) / (totalMs > 0 ? totalMs : 1.0);
        double tokensPerSec = (stats.totalTokensProcessed * 1000.0) / (totalMs > 0 ? totalMs : 1.0);

        std::cout << "  --------------------------------------------------------\n"
                  << "  Corpus Scale         : " << N << " documents\n"
                  << "  Total Duration       : " << std::fixed << std::setprecision(2) << totalMs << " ms\n"
                  << "  Ingestion Throughput : " << std::fixed << std::setprecision(1) << docsPerSec << " docs/sec ("
                  << tokensPerSec << " tokens/sec)\n"
                  << "  Vocabulary Size      : " << stats.uniqueVocabularyCount << " terms ("
                  << index.trieVocabularySize() << " in Trie)\n"
                  << "  Total Postings Count : " << stats.totalPostingsCount << " postings\n"
                  << "  Tokens Processed     : " << stats.totalTokensProcessed << "\n"
                  << "  Integrity Verification: PASSED (0 errors)\n"
                  << "  --------------------------------------------------------" << std::endl;
    };

    // Benchmark 1: 100 documents
    runBenchmarkScale(100);

    // Benchmark 2: 1,000 documents
    runBenchmarkScale(1000);

    // Benchmark 3: 10,000 documents (stress test verifying O(1) posting appends & hash table resizing)
    runBenchmarkScale(10000);
}

} // namespace sse::test
