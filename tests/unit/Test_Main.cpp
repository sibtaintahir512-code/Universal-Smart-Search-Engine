#include <iostream>
#include <cassert>
#include "core/CommonTypes.hpp"

int main() {
    std::cout << "[Test_Main] Running Phase 1 Toolchain Verification Tests..." << std::endl;

    // Verify 24-byte PostingNode layout on 64-bit architectures
    std::cout << "[Test_Main] Checking sizeof(sse::PostingNode): " << sizeof(sse::PostingNode) << " bytes" << std::endl;
#if defined(_M_X64) || defined(__x86_64__) || defined(__aarch64__)
    assert(sizeof(sse::PostingNode) == 24);
    std::cout << "[PASS] PostingNode is verified to be exactly 24 bytes." << std::endl;
#endif

    // Verify DocID default constant
    assert(sse::INVALID_DOC_ID == 0);
    std::cout << "[PASS] INVALID_DOC_ID is verified to be 0." << std::endl;

    // Verify FileMetadata initial values
    sse::FileMetadata meta;
    assert(meta.docId == sse::INVALID_DOC_ID);
    assert(meta.fileSize == 0);
    assert(meta.category == sse::FileCategory::Other);
    assert(meta.extractionState == sse::ExtractionStatus::Pending);
    assert((meta.flags & 0x01) == 0x01);
    std::cout << "[PASS] FileMetadata defaults verified successfully." << std::endl;

    std::cout << "[Test_Main] All Phase 1 toolchain sanity checks PASSED." << std::endl;
    return 0;
}
