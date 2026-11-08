#include "extraction/MediaMetadataExtractor.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cstring>
#include <sstream>
#include <vector>

namespace sse::extraction {

bool MediaMetadataExtractor::supportsExtension(const std::string& ext) const noexcept {
    return (ext == "mp4" || ext == "mkv" || ext == "avi" || ext == "mov" ||
            ext == "webm" || ext == "wmv" || ext == "mp3" || ext == "wav" ||
            ext == "aac" || ext == "flac" || ext == "m4a" || ext == "ogg");
}

bool MediaMetadataExtractor::supportsCategory(FileCategory category) const noexcept {
    return (category == FileCategory::Videos || category == FileCategory::Audio);
}

namespace {
    uint16_t readU16LE(const uint8_t* p) {
        return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
    }
    uint32_t readU32LE(const uint8_t* p) {
        return static_cast<uint32_t>(p[0]) |
               (static_cast<uint32_t>(p[1]) << 8) |
               (static_cast<uint32_t>(p[2]) << 16) |
               (static_cast<uint32_t>(p[3]) << 24);
    }
    uint32_t readU32BE(const uint8_t* p) {
        return (static_cast<uint32_t>(p[0]) << 24) |
               (static_cast<uint32_t>(p[1]) << 16) |
               (static_cast<uint32_t>(p[2]) << 8)  |
               static_cast<uint32_t>(p[3]);
    }
}

bool MediaMetadataExtractor::parseMediaHeader(const std::string& ext,
                                             const uint8_t* data,
                                             std::size_t size,
                                             uint64_t fileSize,
                                             double& outDurationSec,
                                             uint32_t& outWidth,
                                             uint32_t& outHeight,
                                             uint32_t& outSampleRate,
                                             uint16_t& outChannels,
                                             std::string& outTags) {
    outDurationSec = 0.0;
    outWidth = 0;
    outHeight = 0;
    outSampleRate = 0;
    outChannels = 0;
    outTags.clear();

    if (!data || size < 12) return false;

    // 1. WAV Container (RIFF....WAVE)
    if (size >= 44 && data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
        data[8] == 'W' && data[9] == 'A' && data[10] == 'V' && data[11] == 'E') {
        std::size_t offset = 12;
        uint32_t byteRate = 0;
        uint32_t dataBytes = 0;

        while (offset + 8 <= size) {
            std::string chunkId(reinterpret_cast<const char*>(data + offset), 4);
            uint32_t chunkSize = readU32LE(data + offset + 4);

            if (chunkId == "fmt " && offset + 8 + 16 <= size) {
                outChannels = readU16LE(data + offset + 10);
                outSampleRate = readU32LE(data + offset + 12);
                byteRate = readU32LE(data + offset + 16);
            } else if (chunkId == "data") {
                dataBytes = chunkSize;
            }
            offset += 8 + chunkSize;
        }

        if (byteRate > 0 && dataBytes > 0) {
            outDurationSec = static_cast<double>(dataBytes) / static_cast<double>(byteRate);
        }
        return true;
    }

    // 2. MP4 / MOV / M4A (ISO Base Media File Format: ftyp ... moov)
    if (size >= 16 && (ext == "mp4" || ext == "mov" || ext == "m4a")) {
        // Look for mvhd atom
        for (std::size_t i = 0; i + 24 <= size; ++i) {
            if (data[i] == 'm' && data[i + 1] == 'v' && data[i + 2] == 'h' && data[i + 3] == 'd') {
                uint8_t version = data[i + 4];
                if (version == 0 && i + 24 <= size) {
                    uint32_t timeScale = readU32BE(data + i + 16);
                    uint32_t duration = readU32BE(data + i + 20);
                    if (timeScale > 0) {
                        outDurationSec = static_cast<double>(duration) / static_cast<double>(timeScale);
                    }
                }
                break;
            }
        }
        // Approximate width and height from tkhd atom
        for (std::size_t i = 0; i + 84 < size; ++i) {
            if (data[i] == 't' && data[i + 1] == 'k' && data[i + 2] == 'h' && data[i + 3] == 'd') {
                outWidth = readU32BE(data + i + 76) >> 16;
                outHeight = readU32BE(data + i + 80) >> 16;
                if (outWidth > 0 && outHeight > 0) break;
            }
        }
        return (outDurationSec > 0.0 || (outWidth > 0 && outHeight > 0));
    }

    // 3. FLAC (fLaC signature)
    if (size >= 38 && data[0] == 'f' && data[1] == 'L' && data[2] == 'a' && data[3] == 'C') {
        outSampleRate = (static_cast<uint32_t>(data[18]) << 12) |
                        (static_cast<uint32_t>(data[19]) << 4)  |
                        (static_cast<uint32_t>(data[20]) >> 4);
        outChannels = static_cast<uint16_t>(((data[20] >> 1) & 0x07) + 1);

        uint64_t totalSamples = (static_cast<uint64_t>(data[20] & 0x01) << 32) |
                                (static_cast<uint64_t>(data[21]) << 24) |
                                (static_cast<uint64_t>(data[22]) << 16) |
                                (static_cast<uint64_t>(data[23]) << 8)  |
                                static_cast<uint64_t>(data[24]);

        if (outSampleRate > 0) {
            outDurationSec = static_cast<double>(totalSamples) / static_cast<double>(outSampleRate);
        }
        return true;
    }

    // 4. MP3 (ID3v2 tag and MPEG frame sync)
    if (ext == "mp3") {
        std::size_t audioStart = 0;

        // Check for ID3v2 header
        if (size >= 10 && data[0] == 'I' && data[1] == 'D' && data[2] == '3') {
            uint32_t tagSize = (static_cast<uint32_t>(data[6] & 0x7F) << 21) |
                               (static_cast<uint32_t>(data[7] & 0x7F) << 14) |
                               (static_cast<uint32_t>(data[8] & 0x7F) << 7)  |
                               static_cast<uint32_t>(data[9] & 0x7F);

            // Simple title / artist scanner in ID3v2 block
            std::string id3Block(reinterpret_cast<const char*>(data), std::min<std::size_t>(size, tagSize + 10));
            std::size_t titPos = id3Block.find("TIT2");
            if (titPos != std::string::npos && titPos + 10 < id3Block.size()) {
                uint32_t fLen = readU32BE(reinterpret_cast<const uint8_t*>(id3Block.data() + titPos + 4));
                if (fLen > 1 && titPos + 10 + fLen <= id3Block.size()) {
                    outTags += "Title: " + id3Block.substr(titPos + 11, fLen - 1) + "; ";
                }
            }
            audioStart = tagSize + 10;
        }

        // Search for sync word 0xFFE or 0xFFF
        for (std::size_t i = audioStart; i + 4 < size; ++i) {
            if (data[i] == 0xFF && (data[i + 1] & 0xE0) == 0xE0) {
                uint8_t srIdx = (data[i + 2] >> 2) & 0x03;
                static const uint32_t srTable[3] = {44100, 48000, 32000};
                if (srIdx < 3) {
                    outSampleRate = srTable[srIdx];
                }
                outChannels = ((data[i + 3] >> 6) == 3) ? 1 : 2;
                // Estimate duration assuming 128 kbps default if not VBR
                if (fileSize > 0) {
                    outDurationSec = static_cast<double>(fileSize) / (128000.0 / 8.0);
                }
                return true;
            }
        }
        return false;
    }

    return false;
}

ExtractionResult MediaMetadataExtractor::extract(const std::string& filePath,
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
        result.errorDescription = "Media file not found";
        return result;
    }

    uint64_t fileSize = std::filesystem::file_size(fsPath, ec);
    result.originalFileSize = fileSize;

    // Read first 64 KB of container header
    std::ifstream file(fsPath, std::ios::binary);
    if (!file.is_open()) {
        result.status = ExtractionStatus::PermissionDenied;
        result.errorDescription = "Cannot open media file for header inspection";
        return result;
    }

    std::size_t readLen = static_cast<std::size_t>(std::min<uint64_t>(fileSize, 64 * 1024));
    std::vector<uint8_t> header(readLen);
    file.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(readLen));

    std::string ext = fsPath.extension().u8string();
    if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
    for (char& c : ext) c = static_cast<char>(std::tolower(c));

    double duration = 0.0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t sampleRate = 0;
    uint16_t channels = 0;
    std::string tags;

    bool ok = parseMediaHeader(ext, header.data(), header.size(), fileSize,
                               duration, width, height, sampleRate, channels, tags);

    if (ok) {
        std::ostringstream oss;
        if (duration > 0.0) oss << "Duration: " << static_cast<uint64_t>(duration) << "s, ";
        if (width > 0 && height > 0) oss << "Resolution: " << width << "x" << height << ", ";
        if (sampleRate > 0) oss << "SampleRate: " << sampleRate << "Hz, ";
        if (channels > 0) oss << "Channels: " << channels << ", ";
        if (!tags.empty()) oss << "Tags: " << tags;

        result.metadataSummary = oss.str();
        result.status = ExtractionStatus::Success;

        if (!tags.empty()) {
            result.extractedText = tags;
            result.extractedByteCount = tags.size();
        }
    } else {
        result.status = ExtractionStatus::MetadataOnly;
        result.errorDescription = "Container header could not be fully parsed (metadata-only fallback)";
    }

    auto tEnd = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    return result;
}

} // namespace sse::extraction
