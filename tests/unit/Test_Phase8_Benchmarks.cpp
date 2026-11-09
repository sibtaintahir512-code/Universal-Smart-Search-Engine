#include "TestHarness.hpp"
#include "persistence/CRC32.hpp"
#include "persistence/IndexSerializer.hpp"
#include "persistence/IndexDeserializer.hpp"
#include "persistence/IndexCompactor.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include <chrono>
#include <string>
#include <vector>

namespace sse::test {

void runPhase8Benchmarks() {
    std::cout << "[RUNNING] Phase 8 Persistence Performance Benchmarks..." << std::endl;

    using namespace sse::indexing;
    using namespace sse::persistence;

    // 1. CRC-32 Throughput Benchmark (10 MB buffer)
    {
        constexpr std::size_t BUFFER_SIZE = 10 * 1024 * 1024; // 10 MB
        std::string buffer(BUFFER_SIZE, 'A');
        for (std::size_t i = 0; i < BUFFER_SIZE; i += 256) {
            buffer[i] = static_cast<char>(i & 0xFF);
        }

        auto t0 = std::chrono::high_resolution_clock::now();
        uint32_t crc = CRC32::calculate(buffer);
        auto t1 = std::chrono::high_resolution_clock::now();

        double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double throughputMBs = (static_cast<double>(BUFFER_SIZE) / (1024.0 * 1024.0)) / (elapsedMs / 1000.0);

        std::cout << "  [Benchmark] CRC-32 (10 MB buffer): " << elapsedMs << " ms ("
                  << throughputMBs << " MB/s, CRC=0x" << std::hex << crc << std::dec << ")" << std::endl;

        TEST_ASSERT(elapsedMs < 500.0);
    }

    // 2. Synthetic Corpus Serialization & Deserialization Benchmark (10,000 Documents)
    {
        std::cout << "  Generating synthetic corpus of 10000 documents for persistence benchmarks..." << std::endl;
        DocumentCatalog catalog;
        InvertedIndex index;

        constexpr uint32_t CORPUS_SIZE = 10000;
        for (uint32_t i = 1; i <= CORPUS_SIZE; ++i) {
            FileMetadata meta{};
            meta.fileSize = 1024 + (i * 17) % 50000;
            meta.category = FileCategory::Documents;
            meta.extractionState = ExtractionStatus::Extracted;

            std::string path = "/var/corpus/document_" + std::to_string(i) + ".txt";
            std::string name = "document_" + std::to_string(i) + ".txt";
            DocID id = catalog.registerDocument(meta, path, name);

            // Add postings
            index.addTerm("common", id, 1, 3, FIELD_FILENAME | FIELD_CONTENT);

            if (i % 2 == 0) {
                index.addTerm("even", id, 0, 2, FIELD_CONTENT);
            }
            if (i % 10 == 0) {
                index.addTerm("tenth", id, 1, 1, FIELD_FILENAME | FIELD_CONTENT);
            }

            index.documentLengthStatistics().recordDocumentLength(id, 2, 50);
        }

        TEST_ASSERT_EQ(catalog.totalCount(), CORPUS_SIZE);

        // Benchmark Serialization
        BinaryWriter writer;
        auto tSer0 = std::chrono::high_resolution_clock::now();
        PersistenceResult serRes = IndexSerializer::serialize(catalog, index, writer, 1);
        auto tSer1 = std::chrono::high_resolution_clock::now();

        double serMs = std::chrono::duration<double, std::milli>(tSer1 - tSer0).count();
        double serThroughputDocs = static_cast<double>(CORPUS_SIZE) / (serMs / 1000.0);
        double fileSizeKB = static_cast<double>(writer.size()) / 1024.0;

        std::cout << "  [Benchmark] Serialization (10k docs): " << serMs << " ms ("
                  << serThroughputDocs << " docs/s, Size=" << fileSizeKB << " KB)" << std::endl;

        TEST_ASSERT(serRes.isSuccess());
        TEST_ASSERT(serMs < 2000.0);

        // Benchmark Dry-Run Validation
        auto tVal0 = std::chrono::high_resolution_clock::now();
        PersistenceResult valRes = IndexDeserializer::validate(writer.data(), writer.size());
        auto tVal1 = std::chrono::high_resolution_clock::now();

        double valMs = std::chrono::duration<double, std::milli>(tVal1 - tVal0).count();
        std::cout << "  [Benchmark] Full File Validation (10k docs): " << valMs << " ms" << std::endl;
        TEST_ASSERT(valRes.isSuccess());
        TEST_ASSERT(valMs < 500.0);

        // Benchmark Deserialization
        DocumentCatalog restoredCat;
        InvertedIndex restoredIdx;

        auto tDes0 = std::chrono::high_resolution_clock::now();
        PersistenceResult desRes = IndexDeserializer::deserialize(
            writer.data(), writer.size(), restoredCat, restoredIdx);
        auto tDes1 = std::chrono::high_resolution_clock::now();

        double desMs = std::chrono::duration<double, std::milli>(tDes1 - tDes0).count();
        double desThroughputDocs = static_cast<double>(CORPUS_SIZE) / (desMs / 1000.0);

        std::cout << "  [Benchmark] Deserialization & Reconstruction (10k docs): " << desMs << " ms ("
                  << desThroughputDocs << " docs/s)" << std::endl;

        TEST_ASSERT(desRes.isSuccess());
        TEST_ASSERT_EQ(restoredCat.totalCount(), CORPUS_SIZE);
        TEST_ASSERT(desMs < 2000.0);

        // Benchmark Compaction with 20% Tombstoned Documents
        for (DocID id = 1; id <= 2000; ++id) {
            catalog.markDeleted(id);
        }
        TEST_ASSERT_EQ(catalog.activeCount(), 8000U);

        DocumentCatalog compactedCat;
        InvertedIndex compactedIdx;

        auto tComp0 = std::chrono::high_resolution_clock::now();
        PersistenceResult compRes = IndexCompactor::compact(catalog, index, compactedCat, compactedIdx);
        auto tComp1 = std::chrono::high_resolution_clock::now();

        double compMs = std::chrono::duration<double, std::milli>(tComp1 - tComp0).count();
        std::cout << "  [Benchmark] Reference Remapping Compaction (10k docs, 2k tombstones): " << compMs << " ms" << std::endl;

        TEST_ASSERT(compRes.isSuccess());
        TEST_ASSERT_EQ(compactedCat.totalCount(), 8000U);
        TEST_ASSERT(compMs < 1000.0);
    }
}

} // namespace sse::test
