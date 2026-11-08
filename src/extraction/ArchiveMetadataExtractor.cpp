#include "extraction/ArchiveMetadataExtractor.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cstring>
#include <sstream>
#include <vector>

namespace sse::extraction {

bool ArchiveMetadataExtractor::supportsExtension(const std::string& ext) const noexcept {
    return (ext == "zip" || ext == "7z" || ext == "rar" || ext == "tar" || ext == "gz");
}

bool ArchiveMetadataExtractor::supportsCategory(FileCategory category) const noexcept {
    return category == FileCategory::Archives;
}

namespace {
    uint16_t readU16(const uint8_t* p) {
        return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
    }
}

bool ArchiveMetadataExtractor::parseArchiveHeader(const std::string& ext,
                                                 const uint8_t* data,
                                                 std::size_t size,
                                                 uint64_t fileSize,
                                                 uint32_t& outEntryCount,
                                                 uint64_t& outUncompressedSize,
                                                 std::string& outDetails) {
    (void)fileSize;
    outEntryCount = 0;
    outUncompressedSize = 0;
    outDetails.clear();

    if (!data || size < 4) return false;

    // 1. ZIP Archive (PK\x03\x04 or PK\x05\x06)
    if (ext == "zip" || (data[0] == 'P' && data[1] == 'K' && data[2] == 0x03 && data[3] == 0x04)) {
        // Look for EOCD in provided buffer
        int64_t eocdOff = -1;
        for (int64_t i = static_cast<int64_t>(size) - 22; i >= 0; --i) {
            if (data[i] == 'P' && data[i + 1] == 'K' && data[i + 2] == 0x05 && data[i + 3] == 0x06) {
                eocdOff = i;
                break;
            }
        }
        if (eocdOff >= 0) {
            const uint8_t* eocd = data + eocdOff;
            outEntryCount = readU16(eocd + 10);
            outDetails = "Format: ZIP, TotalEntries: " + std::to_string(outEntryCount);
            return true;
        }
        outDetails = "Format: ZIP";
        return true;
    }

    // 2. GZIP Archive (1F 8B)
    if (size >= 10 && data[0] == 0x1F && data[1] == 0x8B) {
        outEntryCount = 1;
        outDetails = "Format: GZIP Single Stream";
        return true;
    }

    // 3. 7-Zip Archive (7z\xBC\xAF\x27\x1C)
    if (size >= 6 && data[0] == '7' && data[1] == 'z' && data[2] == 0xBC &&
        data[3] == 0xAF && data[4] == 0x27 && data[5] == 0x1C) {
        outDetails = "Format: 7-Zip Container";
        return true;
    }

    // 4. RAR Archive (Rar!\x1A\x07)
    if (size >= 7 && data[0] == 'R' && data[1] == 'a' && data[2] == 'r' &&
        data[3] == '!' && data[4] == 0x1A && data[5] == 0x07) {
        outDetails = "Format: RAR Container";
        return true;
    }

    // 5. TAR Archive (ustar check at offset 257)
    if (size >= 512 && std::memcmp(data + 257, "ustar", 5) == 0) {
        outDetails = "Format: POSIX TAR Archive";
        return true;
    }

    return false;
}

ExtractionResult ArchiveMetadataExtractor::extract(const std::string& filePath,
                                                  uint64_t maxBytes,
                                                  const std::atomic<bool>* cancelToken) {
    (void)maxBytes;
    (void)cancelToken;
    auto tStart = std::chrono::steady_clock::now();

    ExtractionResult result;
    result.sourcePath = filePath;
    result.extractorId = EXTRACTOR_ID;
    result.extractorVersion = EXTRACTOR_VERSION;

    std::error_code ec;
    auto fsPath = std::filesystem::u8path(filePath);
    if (!std::filesystem::exists(fsPath, ec) || ec) {
        result.status = ExtractionStatus::Failed;
        result.errorDescription = "Archive file not found";
        return result;
    }

    uint64_t fileSize = std::filesystem::file_size(fsPath, ec);
    result.originalFileSize = fileSize;

    std::ifstream file(fsPath, std::ios::binary);
    if (!file.is_open()) {
        result.status = ExtractionStatus::PermissionDenied;
        result.errorDescription = "Failed to open archive for reading";
        return result;
    }

    // Read initial 64 KB header buffer
    std::size_t readLen = static_cast<std::size_t>(std::min<uint64_t>(fileSize, 64 * 1024));
    std::vector<uint8_t> buffer(readLen);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(readLen));

    std::string ext = fsPath.extension().u8string();
    if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
    for (char& c : ext) c = static_cast<char>(std::tolower(c));

    uint32_t entryCount = 0;
    uint64_t uncompressedSize = 0;
    std::string details;

    bool ok = parseArchiveHeader(ext, buffer.data(), buffer.size(), fileSize,
                                 entryCount, uncompressedSize, details);

    if (ok) {
        std::ostringstream oss;
        oss << details;
        if (entryCount > 0) oss << ", Entries: " << entryCount;
        result.metadataSummary = oss.str();
        // Invariant: Archive contents are NOT extracted to text in Release 1
        result.status = ExtractionStatus::MetadataOnly;
    } else {
        result.status = ExtractionStatus::MetadataOnly;
        result.errorDescription = "Could not parse archive header signature";
    }

    auto tEnd = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    return result;
}

} // namespace sse::extraction
