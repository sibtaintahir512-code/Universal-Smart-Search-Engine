#include "indexing/FileMetadataManager.hpp"
#include <algorithm>
#include <chrono>

namespace sse::indexing {

std::string FileMetadataManager::normalizeExtension(const std::string& rawExt) noexcept {
    std::string ext = rawExt;
    if (!ext.empty() && ext.front() == '.') {
        ext.erase(0, 1);
    }
    for (char& c : ext) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c + ('a' - 'A'));
        }
    }
    return ext;
}

FileCategory FileMetadataManager::classifyExtension(const std::string& rawExt) noexcept {
    std::string ext = normalizeExtension(rawExt);

    // 1. Documents
    if (ext == "txt" || ext == "pdf" || ext == "doc" || ext == "docx" ||
        ext == "ppt" || ext == "pptx" || ext == "xls" || ext == "xlsx" ||
        ext == "rtf" || ext == "md") {
        return FileCategory::Documents;
    }

    // 2. Images
    if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "webp" ||
        ext == "gif" || ext == "bmp" || ext == "svg" || ext == "tiff") {
        return FileCategory::Images;
    }

    // 3. Videos
    if (ext == "mp4" || ext == "mkv" || ext == "avi" || ext == "mov" ||
        ext == "webm" || ext == "wmv") {
        return FileCategory::Videos;
    }

    // 4. Audio
    if (ext == "mp3" || ext == "wav" || ext == "aac" || ext == "flac" ||
        ext == "m4a" || ext == "ogg") {
        return FileCategory::Audio;
    }

    // 5. Programming / Source Code
    if (ext == "cpp" || ext == "c" || ext == "h" || ext == "hpp" ||
        ext == "py" || ext == "java" || ext == "js" || ext == "ts" ||
        ext == "html" || ext == "css" || ext == "sql" || ext == "asm") {
        return FileCategory::SourceCode;
    }

    // 6. Archives
    if (ext == "zip" || ext == "7z" || ext == "rar" || ext == "tar" || ext == "gz") {
        return FileCategory::Archives;
    }

    // 7. Executables
    if (ext == "exe" || ext == "dll" || ext == "msi" || ext == "bat" ||
        ext == "cmd" || ext == "com") {
        return FileCategory::Executables;
    }

    // 8. Other / Unknown
    return FileCategory::Other;
}

std::string FileMetadataManager::detectMimeType(const std::string& rawExt) noexcept {
    std::string ext = normalizeExtension(rawExt);

    if (ext == "txt") return "text/plain";
    if (ext == "md") return "text/markdown";
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css") return "text/css";
    if (ext == "js") return "application/javascript";
    if (ext == "ts") return "application/typescript";
    if (ext == "json") return "application/json";
    if (ext == "xml") return "application/xml";
    if (ext == "pdf") return "application/pdf";
    if (ext == "doc") return "application/msword";
    if (ext == "docx") return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    if (ext == "ppt") return "application/vnd.ms-powerpoint";
    if (ext == "pptx") return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
    if (ext == "xls") return "application/vnd.ms-excel";
    if (ext == "xlsx") return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "webp") return "image/webp";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "bmp") return "image/bmp";
    if (ext == "mp4") return "video/mp4";
    if (ext == "mkv") return "video/x-matroska";
    if (ext == "webm") return "video/webm";
    if (ext == "mp3") return "audio/mpeg";
    if (ext == "wav") return "audio/wav";
    if (ext == "zip") return "application/zip";
    if (ext == "tar") return "application/x-tar";
    if (ext == "gz") return "application/gzip";

    return "application/octet-stream";
}

bool FileMetadataManager::supportsContentExtraction(const std::string& rawExt) noexcept {
    std::string ext = normalizeExtension(rawExt);
    return (ext == "txt" || ext == "md" || ext == "rtf" ||
            ext == "cpp" || ext == "c" || ext == "h" || ext == "hpp" ||
            ext == "py" || ext == "java" || ext == "js" || ext == "ts" ||
            ext == "html" || ext == "css" || ext == "sql" || ext == "asm" ||
            ext == "pdf" || ext == "docx" || ext == "pptx" || ext == "xlsx");
}

uint64_t FileMetadataManager::fileTimeToEpoch(const std::filesystem::file_time_type& ftime) noexcept {
    try {
        // C++17 conversion to system clock
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        auto duration = sctp.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        return (seconds > 0) ? static_cast<uint64_t>(seconds) : 0ULL;
    } catch (...) {
        return 0ULL;
    }
}

bool FileMetadataManager::extractMetadata(const std::filesystem::path& path, ExtendedFileRecord& outRecord) const noexcept {
    std::error_code ec;

    // Check existence without throwing
    bool exists = std::filesystem::exists(path, ec);
    if (ec || !exists) {
        outRecord.metadata.flags &= ~META_FLAG_IS_ACCESSIBLE;
        outRecord.metadata.flags &= ~META_FLAG_COMPLETE;
        return false;
    }

    // Basic file status
    auto status = std::filesystem::status(path, ec);
    if (ec) {
        outRecord.metadata.flags &= ~META_FLAG_IS_ACCESSIBLE;
        return false;
    }

    outRecord.filename = path.filename().u8string();
    outRecord.extension = normalizeExtension(path.extension().u8string());

    // Absolute and canonical path determination
    auto absPath = std::filesystem::absolute(path, ec);
    outRecord.absolutePath = ec ? path.u8string() : absPath.u8string();

    auto canonPath = std::filesystem::weakly_canonical(path, ec);
    outRecord.canonicalPath = ec ? outRecord.absolutePath : canonPath.u8string();

    // Query file size (only for regular files)
    if (std::filesystem::is_regular_file(status)) {
        uint64_t size = std::filesystem::file_size(path, ec);
        outRecord.metadata.fileSize = ec ? 0 : size;
    } else {
        outRecord.metadata.fileSize = 0;
    }

    // Query modification timestamp
    auto lastWrite = std::filesystem::last_write_time(path, ec);
    outRecord.metadata.lastModifiedTime = ec ? 0ULL : fileTimeToEpoch(lastWrite);

    // Creation timestamp (fallback to 0 if not natively exposed via std::filesystem in C++17)
    outRecord.metadata.creationTime = 0ULL;

    // Category and classification
    outRecord.metadata.category = classifyExtension(outRecord.extension);
    outRecord.mimeType = detectMimeType(outRecord.extension);

    // Flags setup
    outRecord.metadata.flags = META_FLAG_ACTIVE | META_FLAG_IS_ACCESSIBLE | META_FLAG_COMPLETE;

    if (std::filesystem::is_symlink(path, ec)) {
        outRecord.metadata.flags |= META_FLAG_IS_SYMLINK;
    }

    if (supportsContentExtraction(outRecord.extension)) {
        outRecord.metadata.flags |= META_FLAG_SUPPORTS_CONTENT_EXTRACT;
        outRecord.metadata.extractionState = ExtractionStatus::Pending;
    } else {
        outRecord.metadata.extractionState = ExtractionStatus::MetadataOnly;
    }

    return true;
}

} // namespace sse::indexing
