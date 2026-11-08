#include <iostream>
#include <iomanip>
#include <cassert>
#include "TestHarness.hpp"
#include "core/CommonTypes.hpp"

namespace sse::test {
    void runDynamicArrayTests();
    void runSinglyLinkedListTests();
    void runHashTableTests();
    void runPostingListTests();
    void runTrieTests();
    void runMaxHeapTests();
    void runMinHeapTests();
    void runStackTests();
    void runQueueTests();
}

int main() {
    std::cout << "======================================================================" << std::endl;
    std::cout << " UNIVERSAL SMART SEARCH ENGINE — PHASE 2 DSA TEST SUITE" << std::endl;
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

    // 2. Run all custom data structure test suites
    std::cout << "\n--- Executing Custom Data Structure Test Suites ---" << std::endl;
    sse::test::runDynamicArrayTests();
    sse::test::runSinglyLinkedListTests();
    sse::test::runHashTableTests();
    sse::test::runPostingListTests();
    sse::test::runTrieTests();
    sse::test::runMaxHeapTests();
    sse::test::runMinHeapTests();
    sse::test::runStackTests();
    sse::test::runQueueTests();

    // 3. Print Final Summary
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
