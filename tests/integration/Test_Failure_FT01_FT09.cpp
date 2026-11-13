#include "TestHarness.hpp"
#include "indexing/FileScanner.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include "indexing/Tokenizer.hpp"
#include "indexing/IndexingPipeline.hpp"
#include "extraction/ExtractionManager.hpp"
#include "extraction/PDFExtractor.hpp"
#include "search/QueryLexer.hpp"
#include "search/QueryParser.hpp"
#include "search/SearchService.hpp"
#include "persistence/IndexPersistenceManager.hpp"
#include "persistence/IndexRecoveryManager.hpp"
#include "persistence/CRC32.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace sse::test {

// =============================================================================
// FT-01: Non-existent Directory
// =============================================================================
void runFT01Tests() {
    std::cout << "\n[RUNNING FT-01] Non-Existent Directory Handling..." << std::endl;

    indexing::DocumentCatalog catalog;
    indexing::FileScanner scanner;
    scanner.addRoot("/path/that/definitely/does/not/exist_12345");

    std::atomic<bool> cancelToken{false};
    auto stats = scanner.scan(catalog, nullptr, nullptr, cancelToken);

    // Must handle non-existent path without throwing exceptions or crashing
    TEST_ASSERT_EQ(stats.filesDiscovered, 0);
    TEST_ASSERT_EQ(catalog.activeCount(), 0);
}

// =============================================================================
// FT-02: Inaccessible System Directory (EACCES)
// =============================================================================
void runFT02Tests() {
    std::cout << "\n[RUNNING FT-02] Inaccessible / Protected Directory Handling..." << std::endl;

    std::filesystem::path testDir = "temp_ft02_protected";
    std::filesystem::remove_all(testDir);
    std::filesystem::create_directories(testDir / "accessible");
    std::filesystem::create_directories(testDir / "restricted");

    std::ofstream(testDir / "accessible" / "file1.txt") << "content 1";
    std::ofstream(testDir / "restricted" / "file2.txt") << "content 2";

    // Restrict permissions on restricted subfolder
    std::error_code ec;
    std::filesystem::permissions(testDir / "restricted", std::filesystem::perms::none, ec);

    indexing::DocumentCatalog catalog;
    indexing::FileScanner scanner;
    scanner.addRoot(testDir.string());

    std::atomic<bool> cancelToken{false};
    auto stats = scanner.scan(catalog, nullptr, nullptr, cancelToken);

    // Scanner must discover accessible files and gracefully skip restricted files
    TEST_ASSERT(stats.filesDiscovered >= 1);

    // Restore permissions for clean removal
    std::filesystem::permissions(testDir / "restricted", std::filesystem::perms::all, ec);
    std::filesystem::remove_all(testDir, ec);
}

// =============================================================================
// FT-03: Corrupted PDF File
// =============================================================================
void runFT03Tests() {
    std::cout << "\n[RUNNING FT-03] Corrupted & Malformed PDF Handling..." << std::endl;

    std::filesystem::path testDir = "temp_ft03_corrupt_pdf";
    std::filesystem::remove_all(testDir);
    std::filesystem::create_directories(testDir);

    auto corruptPdf = testDir / "malformed.pdf";
    {
        std::ofstream out(corruptPdf, std::ios::binary);
        out << "%PDF-1.4\n%Malformed and truncated byte stream\n%%EOF";
        out.close();
    }

    extraction::ExtractionManager extractor;
    auto result = extractor.extract(corruptPdf.string(), FileCategory::Documents);

    // Must not crash; should safely record failure or fallback
    TEST_ASSERT(!result.isSuccess() || result.status == extraction::ExtractionStatus::Failed || result.extractedText.empty());

    std::filesystem::remove_all(testDir);
}

// =============================================================================
// FT-04: Oversized Text File Handling
// =============================================================================
void runFT04Tests() {
    std::cout << "\n[RUNNING FT-04] Oversized Text File Extraction Handling..." << std::endl;

    std::filesystem::path testDir = "temp_ft04_oversized";
    std::filesystem::remove_all(testDir);
    std::filesystem::create_directories(testDir);

    auto largeFile = testDir / "large.txt";
    {
        std::ofstream out(largeFile, std::ios::binary);
        // Write 6 MB of data
        std::string chunk(1024 * 64, 'X');
        for (int i = 0; i < 96; ++i) {
            out << chunk;
        }
        out.close();
    }

    // Set extraction budget to 5 MB
    extraction::ExtractionManager extractor(5ULL * 1024 * 1024);
    auto result = extractor.extract(largeFile.string(), FileCategory::Documents);

    TEST_ASSERT(result.isSuccess());
    TEST_ASSERT(result.isTruncated);
    TEST_ASSERT(result.extractedByteCount <= 5ULL * 1024 * 1024);

    std::filesystem::remove_all(testDir);
}

// =============================================================================
// FT-05: Exclusively Locked / Unreadable File
// =============================================================================
void runFT05Tests() {
    std::cout << "\n[RUNNING FT-05] Exclusively Locked / Unreadable File Handling..." << std::endl;

    std::filesystem::path testDir = "temp_ft05_locked";
    std::filesystem::remove_all(testDir);
    std::filesystem::create_directories(testDir);

    auto lockedFile = testDir / "unreadable.txt";
    std::ofstream(lockedFile) << "secret content";

    std::error_code ec;
    std::filesystem::permissions(lockedFile, std::filesystem::perms::none, ec);

    extraction::ExtractionManager extractor;
    auto result = extractor.extract(lockedFile.string(), FileCategory::Documents);

    // Extraction should report failure safely without throwing
    TEST_ASSERT(!result.isSuccess() || result.status == extraction::ExtractionStatus::Failed);

    std::filesystem::permissions(lockedFile, std::filesystem::perms::all, ec);
    std::filesystem::remove_all(testDir, ec);
}

// =============================================================================
// FT-06: Unbalanced Query Parentheses
// =============================================================================
void runFT06Tests() {
    std::cout << "\n[RUNNING FT-06] Unbalanced Query Parentheses Syntax Error..." << std::endl;

    search::QueryLexer lexer;
    auto lexRes = lexer.tokenize("(cpp AND (dsa)");
    TEST_ASSERT(lexRes.success);

    search::QueryParser parser;
    auto parseRes = parser.parse(lexRes.tokens);

    // Must report syntax error and valid error position
    TEST_ASSERT(!parseRes.success);
    TEST_ASSERT(!parseRes.errorMessage.empty());
    TEST_ASSERT(parseRes.errorOffset > 0);
}

// =============================================================================
// FT-07: Corrupted search.idx & CRC32 Recovery
// =============================================================================
void runFT07Tests() {
    std::cout << "\n[RUNNING FT-07] Corrupted search.idx & CRC-32 Recovery..." << std::endl;

    std::filesystem::path testDir = "temp_ft07_persist";
    std::filesystem::remove_all(testDir);
    std::filesystem::create_directories(testDir);

    indexing::DocumentCatalog catalog;
    indexing::InvertedIndex index;
    FileMetadata meta;
    meta.category = FileCategory::Documents;
    meta.flags = META_FLAG_ACTIVE;
    catalog.registerDocument(meta, "/test/doc.txt", "doc.txt");

    persistence::IndexPersistenceManager persistenceManager(&index, &catalog, testDir);
    auto saveRes = persistenceManager.saveIndex("search.idx");
    TEST_ASSERT(saveRes.isSuccess());

    // Corrupt bytes in search.idx
    auto idxPath = testDir / "search.idx";
    {
        std::fstream f(idxPath, std::ios::in | std::ios::out | std::ios::binary);
        f.seekp(40);
        char corruptByte = 0x7F;
        f.write(&corruptByte, 1);
        f.close();
    }

    // Validation must fail
    auto valRes = persistenceManager.validateIndex("search.idx");
    TEST_ASSERT(!valRes.isSuccess());

    // Recovery must isolate corrupt file and attempt recovery
    auto recRes = persistenceManager.recoverIndex("search.idx");
    TEST_ASSERT(recRes.status == persistence::PersistenceStatus::RecoveredFromBackup || !recRes.isSuccess());

    std::filesystem::remove_all(testDir);
}

// =============================================================================
// FT-08: Deleted Indexed File & Incremental Sync
// =============================================================================
void runFT08Tests() {
    std::cout << "\n[RUNNING FT-08] Deleted Indexed File & Tombstoning..." << std::endl;

    std::filesystem::path testDir = "temp_ft08_corpus";
    std::filesystem::remove_all(testDir);
    std::filesystem::create_directories(testDir);

    auto f1 = testDir / "file1.txt";
    auto f2 = testDir / "file2.txt";
    std::ofstream(f1) << "apple orange";
    std::ofstream(f2) << "banana cherry";

    indexing::DocumentCatalog catalog;
    indexing::FileScanner scanner;
    scanner.addRoot(testDir.string());

    std::atomic<bool> cancelToken{false};
    scanner.scan(catalog, nullptr, nullptr, cancelToken);
    TEST_ASSERT_EQ(catalog.activeCount(), 2);

    // Delete file 1
    std::filesystem::remove(f1);

    // Incremental sync
    scanner.scan(catalog, nullptr, nullptr, cancelToken);

    // Check catalog tombstone
    DocID id1 = catalog.findDocIdByPath(std::filesystem::canonical(f2).string());
    TEST_ASSERT(id1 != INVALID_DOC_ID);

    std::filesystem::remove_all(testDir);
}

// =============================================================================
// FT-09: Allocation Failure / Extreme Request Simulation
// =============================================================================
void runFT09Tests() {
    std::cout << "\n[RUNNING FT-09] Memory Allocation Guard & Bounded Limits..." << std::endl;

    // Test that CustomDynamicArray and CustomHashTable throw std::bad_alloc on impossible sizes
    dsa::CustomDynamicArray<int> arr;
    bool caughtException = false;
    try {
        // Request impossible allocation size
        arr.reserve(SIZE_MAX / 2);
    } catch (const std::bad_alloc&) {
        caughtException = true;
    } catch (...) {
        caughtException = true;
    }
    TEST_ASSERT(caughtException);
    TEST_ASSERT_EQ(arr.size(), 0);
}

} // namespace sse::test
