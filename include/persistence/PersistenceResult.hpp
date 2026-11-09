#pragma once

#include <cstdint>
#include <string>

namespace sse::persistence {

enum class PersistenceStatus {
    Success,
    FileNotFound,
    CorruptedHeader,
    CorruptedSection,
    ChecksumMismatch,
    VersionMismatch,
    IncompatibleTokenizer,
    FileSizeLimitExceeded,
    ResourceLimitExceeded,
    WriteError,
    ReadError,
    RecoveredFromBackup,
    RebuildRequired,
    Cancelled,
    InvalidState
};

inline const char* persistenceStatusToString(PersistenceStatus status) noexcept {
    switch (status) {
        case PersistenceStatus::Success:               return "Success";
        case PersistenceStatus::FileNotFound:          return "File Not Found";
        case PersistenceStatus::CorruptedHeader:       return "Corrupted File Header";
        case PersistenceStatus::CorruptedSection:      return "Corrupted Section Data";
        case PersistenceStatus::ChecksumMismatch:      return "CRC32 Checksum Mismatch";
        case PersistenceStatus::VersionMismatch:       return "Incompatible Format Version";
        case PersistenceStatus::IncompatibleTokenizer: return "Incompatible Tokenizer Configuration";
        case PersistenceStatus::FileSizeLimitExceeded: return "File Size Limit Exceeded";
        case PersistenceStatus::ResourceLimitExceeded: return "Resource Limit Exceeded";
        case PersistenceStatus::WriteError:            return "Disk Write Error";
        case PersistenceStatus::ReadError:             return "Disk Read Error";
        case PersistenceStatus::RecoveredFromBackup:   return "Successfully Recovered From Backup";
        case PersistenceStatus::RebuildRequired:       return "Corrupt Index - Rebuild Required";
        case PersistenceStatus::Cancelled:             return "Operation Cancelled";
        case PersistenceStatus::InvalidState:          return "Invalid Subsystem State";
        default:                                       return "Unknown Error";
    }
}

/**
 * @brief Structured result DTO reporting persistence operation outcomes.
 */
struct PersistenceResult {
    PersistenceStatus status{PersistenceStatus::Success};
    std::string       message;
    uint64_t          bytesProcessed{0};
    uint32_t          documentCount{0};
    uint32_t          activeDocumentCount{0};
    uint32_t          vocabularySize{0};
    uint64_t          postingsCount{0};
    double            durationMs{0.0};
    uint64_t          generation{0};

    bool isSuccess() const noexcept {
        return status == PersistenceStatus::Success ||
               status == PersistenceStatus::RecoveredFromBackup;
    }

    static PersistenceResult success(std::string msg = "Operation completed successfully") {
        PersistenceResult res;
        res.status = PersistenceStatus::Success;
        res.message = std::move(msg);
        return res;
    }

    static PersistenceResult failure(PersistenceStatus status, std::string msg) {
        PersistenceResult res;
        res.status = status;
        res.message = std::move(msg);
        return res;
    }
};

} // namespace sse::persistence
