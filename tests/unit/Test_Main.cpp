#include <iostream>
#include <iomanip>
#include <cassert>
#include <QApplication>
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
    void runPhase41HardeningTests();
    void runPhase42ComplianceTests();

    // Phase 5 Inverted Index & Tokenizer Pipeline Test Suites
    void runTextNormalizerTests();
    void runTokenizerTests();
    void runInvertedIndexTests();
    void runIndexingPipelineTests();
    void runIndexIntegrityTests();
    void runPhase5BenchmarkTests();

    // Phase 6 Search Engine & Boolean Query Processing Test Suites
    void runQueryLexerTests();
    void runQueryParserTests();
    void runQueryPrecedenceTests();
    void runResultSetTests();
    void runBooleanEvaluatorTests();
    void runFieldSearchTests();
    void runSearchPaginationTests();
    void runSearchCancellationTests();
    void runSearchIntegrityTests();
    void runSearchServiceTests();
    void runPhase6BenchmarkTests();

    // Phase 7 Relevance Ranking & Smart Search
    void runDocumentLengthStatisticsTests();
    void runTFIDFTests();
    void runBM25Tests();
    void runFieldWeightsTests();
    void runTopKSelectorTests();
    void runRankingEngineTests();
    void runRankingDeterminismTests();
    void runBooleanAwareRankingTests();
    void runRankingUpdatesTests();
    void runRankingPaginationTests();
    void runRankingExplanationTests();
    void runRankingStatisticsTests();
    void runPhase7BenchmarkTests();

    // Phase 8 Binary Index Serialization & Persistence
    void runCRC32Tests();
    void runBinaryIOTests();
    void runIndexFileHeaderTests();
    void runSectionDirectoryTests();
    void runIndexSerializationTests();
    void runPersistenceIntegrityTests();
    void runAtomicSaveTests();
    void runIndexRecoveryTests();
    void runIndexCompactionTests();
    void runRoundTripEquivalenceTests();
    void runPhase8Benchmarks();

    // Phase 9 Qt 6 Desktop GUI & Background Workers
    void runSearchResultModelTests();
    void runSearchEngineAdapterTests();
    void runThemeManagerTests();
    void runSearchWorkerTests();
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
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
    sse::test::runPhase41HardeningTests();
    sse::test::runPhase42ComplianceTests();

    // 5. Run Phase 5 Inverted Index & Tokenizer Test Suites
    std::cout << "\n--- Executing Phase 5 Inverted Index & Tokenizer Test Suites ---" << std::endl;
    sse::test::runTextNormalizerTests();
    sse::test::runTokenizerTests();
    sse::test::runInvertedIndexTests();
    sse::test::runIndexingPipelineTests();
    sse::test::runIndexIntegrityTests();
    sse::test::runPhase5BenchmarkTests();

    // 6. Run Phase 6 Search Engine & Boolean Query Processing Test Suites
    std::cout << "\n--- Executing Phase 6 Search Engine & Boolean Query Processing Test Suites ---" << std::endl;
    sse::test::runQueryLexerTests();
    sse::test::runQueryParserTests();
    sse::test::runQueryPrecedenceTests();
    sse::test::runResultSetTests();
    sse::test::runBooleanEvaluatorTests();
    sse::test::runFieldSearchTests();
    sse::test::runSearchPaginationTests();
    sse::test::runSearchCancellationTests();
    sse::test::runSearchIntegrityTests();
    sse::test::runSearchServiceTests();
    sse::test::runPhase6BenchmarkTests();

    // 7. Run Phase 7 Relevance Ranking & Smart Search Test Suites
    std::cout << "\n--- Executing Phase 7 Relevance Ranking & Smart Search Test Suites ---" << std::endl;
    sse::test::runDocumentLengthStatisticsTests();
    sse::test::runTFIDFTests();
    sse::test::runBM25Tests();
    sse::test::runFieldWeightsTests();
    sse::test::runTopKSelectorTests();
    sse::test::runRankingEngineTests();
    sse::test::runRankingDeterminismTests();
    sse::test::runBooleanAwareRankingTests();
    sse::test::runRankingUpdatesTests();
    sse::test::runRankingPaginationTests();
    sse::test::runRankingExplanationTests();
    sse::test::runRankingStatisticsTests();
    sse::test::runPhase7BenchmarkTests();

    // 8. Run Phase 8 Binary Index Serialization & Persistence Test Suites
    std::cout << "\n--- Executing Phase 8 Binary Index Serialization & Persistence Test Suites ---" << std::endl;
    sse::test::runCRC32Tests();
    sse::test::runBinaryIOTests();
    sse::test::runIndexFileHeaderTests();
    sse::test::runSectionDirectoryTests();
    sse::test::runIndexSerializationTests();
    sse::test::runPersistenceIntegrityTests();
    sse::test::runAtomicSaveTests();
    sse::test::runIndexRecoveryTests();
    sse::test::runIndexCompactionTests();
    sse::test::runRoundTripEquivalenceTests();
    sse::test::runPhase8Benchmarks();

    // 9. Run Phase 9 Qt 6 Desktop GUI & Background Worker Test Suites
    std::cout << "\n--- Executing Phase 9 Qt 6 Desktop GUI & Background Worker Test Suites ---" << std::endl;
    sse::test::runSearchResultModelTests();
    sse::test::runSearchEngineAdapterTests();
    sse::test::runThemeManagerTests();
    sse::test::runSearchWorkerTests();

    // 10. Print Final Summary
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
