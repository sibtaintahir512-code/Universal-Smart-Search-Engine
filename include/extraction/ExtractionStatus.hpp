#pragma once

#include <cstdint>
#include <string>

namespace sse::extraction {

/**
 * @brief Granular status of a content extraction attempt.
 */
enum class ExtractionStatus : uint8_t {
    Success = 0,        // Full text or metadata extracted successfully
    Partial,            // Extraction stopped at configurable budget limit (Truncated)
    MetadataOnly,       // Content extraction intentionally omitted (e.g. format type or > 1GB)
    UnsupportedFormat,  // File format not supported by available decoders
    Corrupted,          // Malformed container, unparseable binary, or decompression failure
    Encrypted,          // Password-protected or encrypted document
    PermissionDenied,   // Operating system denied read access / locking violation
    FileTooLarge,       // File exceeds maximum allowable threshold (> 1GB)
    Cancelled,          // Extraction aborted by user or scanner cancellation token
    Failed              // General I/O or decoding failure
};

inline const char* toString(ExtractionStatus status) noexcept {
    switch (status) {
        case ExtractionStatus::Success: return "Success";
        case ExtractionStatus::Partial: return "Partial";
        case ExtractionStatus::MetadataOnly: return "MetadataOnly";
        case ExtractionStatus::UnsupportedFormat: return "UnsupportedFormat";
        case ExtractionStatus::Corrupted: return "Corrupted";
        case ExtractionStatus::Encrypted: return "Encrypted";
        case ExtractionStatus::PermissionDenied: return "PermissionDenied";
        case ExtractionStatus::FileTooLarge: return "FileTooLarge";
        case ExtractionStatus::Cancelled: return "Cancelled";
        case ExtractionStatus::Failed: return "Failed";
    }
    return "Unknown";
}

} // namespace sse::extraction
