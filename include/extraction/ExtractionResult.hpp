#pragma once

#include <cstdint>
#include <string>
#include "core/CommonTypes.hpp"
#include "extraction/ExtractionStatus.hpp"

namespace sse::extraction {

/**
 * @brief Unified result data object returned by all content and metadata extractors.
 */
struct ExtractionResult {
    DocID            docId{INVALID_DOC_ID};
    std::string      sourcePath;
    std::string      extractedText;
    ExtractionStatus status{ExtractionStatus::Failed};
    uint64_t         extractedByteCount{0};
    uint64_t         originalFileSize{0};
    double           durationMs{0.0};
    bool             isTruncated{false};
    int              errorCode{0};
    std::string      errorDescription;
    std::string      extractorId;
    std::string      extractorVersion;
    std::string      metadataSummary;

    ExtractionResult() = default;

    bool isSuccess() const noexcept {
        return status == ExtractionStatus::Success || status == ExtractionStatus::Partial;
    }

    bool hasText() const noexcept {
        return !extractedText.empty();
    }

    std::string toString() const;
};

} // namespace sse::extraction
