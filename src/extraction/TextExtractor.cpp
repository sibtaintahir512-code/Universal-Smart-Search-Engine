#include "extraction/TextExtractor.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <vector>

namespace sse::extraction {

bool TextExtractor::supportsExtension(const std::string& ext) const noexcept {
    return (ext == "txt" || ext == "md" || ext == "markdown" || ext == "text" || ext == "log");
}

bool TextExtractor::supportsCategory(FileCategory category) const noexcept {
    return category == FileCategory::Documents;
}

namespace {
    // Helper to append a Unicode codepoint to a UTF-8 string
    void appendCodePointToUtf8(uint32_t cp, std::string& out) {
        if (cp <= 0x7F) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0x10FFFF) {
            out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            // Replacement char U+FFFD
            out.append("\xEF\xBF\xBD");
        }
    }
}

std::string TextExtractor::decodeToUtf8(const char* data, std::size_t size) {
    if (!data || size == 0) {
        return "";
    }

    const auto* udata = reinterpret_cast<const uint8_t*>(data);

    // 1. Check for UTF-8 BOM (EF BB BF)
    if (size >= 3 && udata[0] == 0xEF && udata[1] == 0xBB && udata[2] == 0xBF) {
        return std::string(data + 3, size - 3);
    }

    // 2. Check for UTF-16 Little Endian BOM (FF FE)
    if (size >= 2 && udata[0] == 0xFF && udata[1] == 0xFE) {
        std::string utf8;
        utf8.reserve((size - 2));
        for (std::size_t i = 2; i + 1 < size; i += 2) {
            uint16_t unit = static_cast<uint16_t>(udata[i] | (static_cast<uint16_t>(udata[i + 1]) << 8));
            appendCodePointToUtf8(unit, utf8);
        }
        return utf8;
    }

    // 3. Check for UTF-16 Big Endian BOM (FE FF)
    if (size >= 2 && udata[0] == 0xFE && udata[1] == 0xFF) {
        std::string utf8;
        utf8.reserve((size - 2));
        for (std::size_t i = 2; i + 1 < size; i += 2) {
            uint16_t unit = static_cast<uint16_t>((static_cast<uint16_t>(udata[i]) << 8) | udata[i + 1]);
            appendCodePointToUtf8(unit, utf8);
        }
        return utf8;
    }

    // 4. Default: Standard UTF-8 / ASCII validation
    std::string result;
    result.reserve(size);

    std::size_t i = 0;
    while (i < size) {
        uint8_t byte = udata[i];
        if (byte <= 0x7F) {
            result.push_back(static_cast<char>(byte));
            ++i;
        } else if ((byte & 0xE0) == 0xC0) {
            if (i + 1 < size && (udata[i + 1] & 0xC0) == 0x80) {
                result.push_back(static_cast<char>(byte));
                result.push_back(static_cast<char>(udata[i + 1]));
                i += 2;
            } else {
                result.append("\xEF\xBF\xBD"); // Replacement char
                ++i;
            }
        } else if ((byte & 0xF0) == 0xE0) {
            if (i + 2 < size && (udata[i + 1] & 0xC0) == 0x80 && (udata[i + 2] & 0xC0) == 0x80) {
                result.push_back(static_cast<char>(byte));
                result.push_back(static_cast<char>(udata[i + 1]));
                result.push_back(static_cast<char>(udata[i + 2]));
                i += 3;
            } else {
                result.append("\xEF\xBF\xBD");
                ++i;
            }
        } else if ((byte & 0xF8) == 0xF0) {
            if (i + 3 < size && (udata[i + 1] & 0xC0) == 0x80 &&
                (udata[i + 2] & 0xC0) == 0x80 && (udata[i + 3] & 0xC0) == 0x80) {
                result.push_back(static_cast<char>(byte));
                result.push_back(static_cast<char>(udata[i + 1]));
                result.push_back(static_cast<char>(udata[i + 2]));
                result.push_back(static_cast<char>(udata[i + 3]));
                i += 4;
            } else {
                result.append("\xEF\xBF\xBD");
                ++i;
            }
        } else {
            result.append("\xEF\xBF\xBD");
            ++i;
        }
    }

    return result;
}

ExtractionResult TextExtractor::extract(const std::string& filePath,
                                       uint64_t maxBytes,
                                       const std::atomic<bool>* cancelToken) {
    auto tStart = std::chrono::steady_clock::now();

    ExtractionResult result;
    result.sourcePath = filePath;
    result.extractorId = EXTRACTOR_ID;
    result.extractorVersion = EXTRACTOR_VERSION;

    std::error_code ec;
    auto fsPath = std::filesystem::u8path(filePath);
    bool exists = std::filesystem::exists(fsPath, ec);
    if (ec || !exists) {
        result.status = ExtractionStatus::Failed;
        result.errorDescription = "File does not exist or path is invalid";
        return result;
    }

    uint64_t fileSize = std::filesystem::file_size(fsPath, ec);
    if (ec) {
        result.status = ExtractionStatus::PermissionDenied;
        result.errorDescription = "Unable to query file size";
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
        result.errorDescription = "Failed to open file for read access (sharing violation or permissions)";
        return result;
    }

    uint64_t bytesToRead = std::min(fileSize, maxBytes);
    bool truncated = (fileSize > maxBytes);

    std::vector<char> rawBuffer(static_cast<std::size_t>(bytesToRead));
    constexpr std::size_t CHUNK_SIZE = 64 * 1024; // 64 KB chunking for cancellation checks
    std::size_t bytesReadTotal = 0;

    while (bytesReadTotal < bytesToRead) {
        if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
            result.status = ExtractionStatus::Cancelled;
            result.errorDescription = "Extraction cancelled by user";
            return result;
        }

        std::size_t chunkSize = std::min<std::size_t>(CHUNK_SIZE, static_cast<std::size_t>(bytesToRead - bytesReadTotal));
        file.read(rawBuffer.data() + bytesReadTotal, static_cast<std::streamsize>(chunkSize));
        std::streamsize actual = file.gcount();
        if (actual <= 0) break;
        bytesReadTotal += static_cast<std::size_t>(actual);
    }

    result.extractedText = decodeToUtf8(rawBuffer.data(), bytesReadTotal);
    result.extractedByteCount = result.extractedText.size();
    result.isTruncated = truncated;
    result.status = truncated ? ExtractionStatus::Partial : ExtractionStatus::Success;

    auto tEnd = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    return result;
}

} // namespace sse::extraction
