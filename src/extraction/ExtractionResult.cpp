#include "extraction/ExtractionResult.hpp"
#include <sstream>

namespace sse::extraction {

std::string ExtractionResult::toString() const {
    std::ostringstream oss;
    oss << "[ExtractionResult] Path: \"" << sourcePath << "\""
        << ", Status: " << sse::extraction::toString(status)
        << ", ExtractedBytes: " << extractedByteCount
        << ", OriginalSize: " << originalFileSize
        << ", Duration: " << durationMs << " ms"
        << ", Truncated: " << (isTruncated ? "true" : "false")
        << ", Extractor: " << extractorId << " v" << extractorVersion;
    if (!errorDescription.empty()) {
        oss << ", Error: " << errorDescription;
    }
    if (!metadataSummary.empty()) {
        oss << ", Metadata: {" << metadataSummary << "}";
    }
    return oss.str();
}

} // namespace sse::extraction
