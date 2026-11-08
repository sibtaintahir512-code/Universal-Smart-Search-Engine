#include "extraction/SourceCodeExtractor.hpp"
#include "extraction/TextExtractor.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <vector>

namespace sse::extraction {

bool SourceCodeExtractor::supportsExtension(const std::string& ext) const noexcept {
    return (ext == "cpp" || ext == "c" || ext == "h" || ext == "hpp" ||
            ext == "cc" || ext == "cxx" || ext == "py" || ext == "java" ||
            ext == "js" || ext == "ts" || ext == "html" || ext == "htm" ||
            ext == "css" || ext == "sql" || ext == "asm" || ext == "json" ||
            ext == "xml" || ext == "cmake" || ext == "sh" || ext == "bat");
}

bool SourceCodeExtractor::supportsCategory(FileCategory category) const noexcept {
    return category == FileCategory::SourceCode;
}

ExtractionResult SourceCodeExtractor::extract(const std::string& filePath,
                                             uint64_t maxBytes,
                                             const std::atomic<bool>* cancelToken) {
    auto tStart = std::chrono::steady_clock::now();

    ExtractionResult result;
    result.sourcePath = filePath;
    result.extractorId = EXTRACTOR_ID;
    result.extractorVersion = EXTRACTOR_VERSION;

    std::error_code ec;
    auto fsPath = std::filesystem::u8path(filePath);
    if (!std::filesystem::exists(fsPath, ec) || ec) {
        result.status = ExtractionStatus::Failed;
        result.errorDescription = "Source code file not found";
        return result;
    }

    uint64_t fileSize = std::filesystem::file_size(fsPath, ec);
    if (ec) {
        result.status = ExtractionStatus::PermissionDenied;
        result.errorDescription = "Failed to query source code file size";
        return result;
    }
    result.originalFileSize = fileSize;

    if (fileSize == 0) {
        result.status = ExtractionStatus::Success;
        result.extractedByteCount = 0;
        result.extractedText = "";
        result.isTruncated = false;
        auto tEnd = std::chrono::steady_clock::now();
        result.durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        return result;
    }

    std::ifstream file(fsPath, std::ios::binary);
    if (!file.is_open()) {
        result.status = ExtractionStatus::PermissionDenied;
        result.errorDescription = "Cannot open source file for reading";
        return result;
    }

    uint64_t bytesToRead = std::min(fileSize, maxBytes);
    bool truncated = (fileSize > maxBytes);

    std::vector<char> rawBuffer(static_cast<std::size_t>(bytesToRead));
    constexpr std::size_t CHUNK_SIZE = 64 * 1024;
    std::size_t totalRead = 0;

    while (totalRead < bytesToRead) {
        if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
            result.status = ExtractionStatus::Cancelled;
            result.errorDescription = "Extraction cancelled by user";
            return result;
        }

        std::size_t chunk = std::min<std::size_t>(CHUNK_SIZE, static_cast<std::size_t>(bytesToRead - totalRead));
        file.read(rawBuffer.data() + totalRead, static_cast<std::streamsize>(chunk));
        std::streamsize actual = file.gcount();
        if (actual <= 0) break;
        totalRead += static_cast<std::size_t>(actual);
    }

    // Decode preserving line structure and indentations
    result.extractedText = TextExtractor::decodeToUtf8(rawBuffer.data(), totalRead);
    result.extractedByteCount = result.extractedText.size();
    result.isTruncated = truncated;
    result.status = truncated ? ExtractionStatus::Partial : ExtractionStatus::Success;

    auto tEnd = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    return result;
}

} // namespace sse::extraction
