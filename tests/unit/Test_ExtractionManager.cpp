#include "TestHarness.hpp"
#include "extraction/ExtractionManager.hpp"
#include "indexing/DocumentCatalog.hpp"
#include <fstream>
#include <filesystem>

namespace sse::test {

void runExtractionManagerTests() {
    std::cout << "[RUNNING] ExtractionManager Tests..." << std::endl;

    using namespace sse::extraction;
    using namespace sse::indexing;
    using sse::extraction::ExtractionStatus;

    auto tempDir = std::filesystem::temp_directory_path() / "sse_mgr_test";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);

    ExtractionManager manager;

    // Test 1: Budget Clamp
    {
        manager.setMaxExtractionBytes(1024); // Below 5MB min -> clamped to 5MB
        TEST_ASSERT_EQ(manager.getMaxExtractionBytes(), 5ULL * 1024 * 1024);

        manager.setMaxExtractionBytes(500ULL * 1024 * 1024); // Above 100MB max -> clamped to 100MB
        TEST_ASSERT_EQ(manager.getMaxExtractionBytes(), 100ULL * 1024 * 1024);

        manager.setMaxExtractionBytes(50ULL * 1024 * 1024); // Valid 50MB
        TEST_ASSERT_EQ(manager.getMaxExtractionBytes(), 50ULL * 1024 * 1024);
    }

    // Test 2: Dispatch and Caching on Real Text File
    {
        auto fTxt = tempDir / "report.txt";
        { std::ofstream(fTxt) << "Executive Summary for Search Engine"; }

        // First call: Cache miss -> extracts and puts in cache
        ExtractionResult r1 = manager.extract(fTxt.u8string(), FileCategory::Documents, 1);
        TEST_ASSERT(r1.isSuccess());
        TEST_ASSERT_EQ(r1.extractedText, std::string("Executive Summary for Search Engine"));
        TEST_ASSERT_EQ(r1.extractorId, std::string("TextExtractor"));
        TEST_ASSERT_EQ(manager.getCache().size(), 1);

        // Second call: Cache hit -> returns cached result without re-reading
        ExtractionResult r2 = manager.extract(fTxt.u8string(), FileCategory::Documents, 1);
        TEST_ASSERT(r2.isSuccess());
        TEST_ASSERT_EQ(r2.extractedText, std::string("Executive Summary for Search Engine"));
    }

    // Test 3: DocumentCatalog Integration
    {
        auto fCode = tempDir / "algorithm.cpp";
        { std::ofstream(fCode) << "void quicksort() {}"; }

        DocumentCatalog catalog;
        FileMetadata meta;
        meta.fileSize = 20;
        meta.category = FileCategory::SourceCode;
        DocID id = catalog.registerDocument(meta, fCode.u8string(), "algorithm.cpp");

        ExtractionResult res = manager.extractDocument(id, catalog);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT_EQ(res.docId, id);
        TEST_ASSERT_EQ(res.extractedText, std::string("void quicksort() {}"));
        TEST_ASSERT_EQ(res.extractorId, std::string("SourceCodeExtractor"));
    }

    // Test 4: Unsupported Format
    {
        auto fUnknown = tempDir / "unknown.randomext999";
        { std::ofstream(fUnknown) << "some random binary"; }

        ExtractionResult res = manager.extract(fUnknown.u8string(), FileCategory::Other, 5);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::UnsupportedFormat);
    }

    // Test 5: Cancellation Token
    {
        auto fText = tempDir / "cancel_mgr.txt";
        { std::ofstream(fText) << "cancel test"; }

        std::atomic<bool> cancelToken{true};
        ExtractionResult res = manager.extract(fText.u8string(), FileCategory::Documents, 1, &cancelToken);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Cancelled);
    }

    std::filesystem::remove_all(tempDir);
}

} // namespace sse::test
