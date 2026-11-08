#include <iostream>
#include <iomanip>
#include <cassert>
#include "TestHarness.hpp"
#include "core/CommonTypes.hpp"

namespace sse::test {
    // Phase 2 DSA Test Suites
    void runDynamicArrayTests();
    void runSinglyLinkedListTests();
    void runHashTableTests();
    void runPostingListTests();
    void runTrieTests();
    void runMaxHeapTests();
    void runMinHeapTests();
    void runStackTests();
    void runQueueTests();

    // Phase 3 Scanner & Catalog Test Suites
    void runFileMetadataManagerTests();
    void runDocumentCatalogTests();
    void runFileChangeDetectorTests();
    void runScanStatisticsTests();
    void runFileScannerTests();
    void runScanWorkerTests();

    // Phase 4 Content & Metadata Extraction Test Suites
    void runTextExtractorTests();
    void runSourceCodeExtractorTests();
    void runOfficeExtractorTests();
    void runPDFExtractorTests();
    void runImageMetadataExtractorTests();
    void runMediaMetadataExtractorTests();
    void runArchiveMetadataExtractorTests();
    void runExtractionCacheTests();
    void runExtractionManagerTests();
}

int main() {
    std::cout << "======================================================================" << std::endl;
    std::cout << " UNIVERSAL SMART SEARCH ENGINE — INTEGRATED TEST SUITE" << std::endl;
    std::cout << "======================================================================" << std::endl;

    // 1. Verify CommonTypes & 24-byte PostingNode layout
    std::cout << "\n[SANITY CHECK] Validating Memory Layout & System Types..." << std::endl;
    std::cout << "  sizeof(sse::PostingNode): " << sizeof(sse::PostingNode) << " bytes" << std::endl;
#if defined(_M_X64) || defined(__x86_64__) || defined(__aarch64__)
    TEST_ASSERT(sizeof(sse::PostingNode) == 24);
#endif
    TEST_ASSERT(sse::INVALID_DOC_ID == 0);
    sse::FileMetadata meta;
    TEST_ASSERT(meta.docId == sse::INVALID_DOC_ID);
    TEST_ASSERT((meta.flags & 0x01) == 0x01);

    // 2. Run Phase 2 custom data structure test suites
    std::cout << "\n--- Executing Phase 2 Custom DSA Test Suites ---" << std::endl;
    sse::test::runDynamicArrayTests();
    sse::test::runSinglyLinkedListTests();
    sse::test::runHashTableTests();
    sse::test::runPostingListTests();
    sse::test::runTrieTests();
    sse::test::runMaxHeapTests();
    sse::test::runMinHeapTests();
    sse::test::runStackTests();
    sse::test::runQueueTests();

    // 3. Run Phase 3 File Discovery & Cataloging Test Suites
    std::cout << "\n--- Executing Phase 3 File Discovery & Catalog Test Suites ---" << std::endl;
    sse::test::runFileMetadataManagerTests();
    sse::test::runDocumentCatalogTests();
    sse::test::runFileChangeDetectorTests();
    sse::test::runScanStatisticsTests();
    sse::test::runFileScannerTests();
    sse::test::runScanWorkerTests();

    // 4. Run Phase 4 Content & Metadata Extraction Test Suites
    std::cout << "\n--- Executing Phase 4 Content & Metadata Extraction Test Suites ---" << std::endl;
    sse::test::runTextExtractorTests();
    sse::test::runSourceCodeExtractorTests();
    sse::test::runOfficeExtractorTests();
    sse::test::runPDFExtractorTests();
    sse::test::runImageMetadataExtractorTests();
    sse::test::runMediaMetadataExtractorTests();
    sse::test::runArchiveMetadataExtractorTests();
    sse::test::runExtractionCacheTests();
    sse::test::runExtractionManagerTests();

    // 5. Print Final Summary
    std::cout << "\n======================================================================" << std::endl;
    std::cout << " TEST EXECUTION SUMMARY" << std::endl;
    std::cout << "======================================================================" << std::endl;
    std::cout << "  Total Assertions Checked : " << sse::test::g_totalTests << std::endl;
    std::cout << "  Assertions Passed        : " << sse::test::g_passedTests << std::endl;
    std::cout << "  Assertions Failed        : " << sse::test::g_failedTests << std::endl;

    if (sse::test::g_failedTests == 0) {
        std::cout << "\n >>> ALL " << sse::test::g_totalTests << " ASSERTIONS PASSED SUCCESSFULLY! (0 Failures) <<<" << std::endl;
        std::cout << "======================================================================" << std::endl;
        return 0;
    } else {
        std::cerr << "\n >>> FAILURE: " << sse::test::g_failedTests << " ASSERTIONS FAILED! <<<" << std::endl;
        std::cout << "======================================================================" << std::endl;
        return 1;
    }
}
