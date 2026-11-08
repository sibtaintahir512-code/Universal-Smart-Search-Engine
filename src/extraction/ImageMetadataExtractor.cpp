#include "extraction/ImageMetadataExtractor.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cstring>
#include <sstream>
#include <vector>

namespace sse::extraction {

bool ImageMetadataExtractor::supportsExtension(const std::string& ext) const noexcept {
    return (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "webp" ||
            ext == "gif" || ext == "bmp" || ext == "svg" || ext == "tiff" || ext == "tif");
}

bool ImageMetadataExtractor::supportsCategory(FileCategory category) const noexcept {
    return category == FileCategory::Images;
}

namespace {
    uint16_t readU16BE(const uint8_t* p) {
        return static_cast<uint16_t>((p[0] << 8) | p[1]);
    }
    uint16_t readU16LE(const uint8_t* p) {
        return static_cast<uint16_t>(p[0] | (p[1] << 8));
    }
    uint32_t readU32BE(const uint8_t* p) {
        return (static_cast<uint32_t>(p[0]) << 24) |
               (static_cast<uint32_t>(p[1]) << 16) |
               (static_cast<uint32_t>(p[2]) << 8)  |
               static_cast<uint32_t>(p[3]);
    }
    uint32_t readU32LE(const uint8_t* p) {
        return static_cast<uint32_t>(p[0]) |
               (static_cast<uint32_t>(p[1]) << 8) |
               (static_cast<uint32_t>(p[2]) << 16) |
               (static_cast<uint32_t>(p[3]) << 24);
    }
}

bool ImageMetadataExtractor::extractDimensions(const std::string& ext,
                                              const uint8_t* data,
                                              std::size_t size,
                                              uint32_t& outWidth,
                                              uint32_t& outHeight,
                                              std::string& outDetails) {
    outWidth = 0;
    outHeight = 0;
    outDetails.clear();

    if (!data || size < 8) return false;

    // 1. PNG Header
    if (size >= 24 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
        outWidth = readU32BE(data + 16);
        outHeight = readU32BE(data + 20);
        uint8_t bitDepth = (size >= 25) ? data[24] : 8;
        outDetails = "Format: PNG, BitDepth: " + std::to_string(bitDepth);
        return (outWidth > 0 && outHeight > 0);
    }

    // 2. GIF Header
    if (size >= 10 && data[0] == 'G' && data[1] == 'I' && data[2] == 'F') {
        outWidth = readU16LE(data + 6);
        outHeight = readU16LE(data + 8);
        outDetails = "Format: GIF";
        return (outWidth > 0 && outHeight > 0);
    }

    // 3. BMP Header
    if (size >= 26 && data[0] == 'B' && data[1] == 'M') {
        outWidth = readU32LE(data + 18);
        outHeight = readU32LE(data + 22);
        outDetails = "Format: BMP";
        return (outWidth > 0 && outHeight > 0);
    }

    // 4. JPEG Header (Marker scanning)
    if (size >= 4 && data[0] == 0xFF && data[1] == 0xD8) {
        std::size_t i = 2;
        while (i + 4 < size) {
            if (data[i] != 0xFF) {
                ++i;
                continue;
            }
            uint8_t marker = data[i + 1];
            if (marker == 0xC0 || marker == 0xC1 || marker == 0xC2) { // Baseline / Progressive SOF
                if (i + 8 < size) {
                    outHeight = readU16BE(data + i + 5);
                    outWidth = readU16BE(data + i + 7);
                    outDetails = "Format: JPEG";
                    return (outWidth > 0 && outHeight > 0);
                }
            }
            // Skip marker segment
            if (marker == 0xD9 || marker == 0xDA) break; // SOS or EOI
            uint16_t len = readU16BE(data + i + 2);
            if (len < 2) break;
            i += 2 + len;
        }
    }

    // 5. WebP Header
    if (size >= 30 && data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
        data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P') {
        if (data[12] == 'V' && data[13] == 'P' && data[14] == '8' && data[15] == ' ') {
            if (size >= 30) {
                outWidth = readU16LE(data + 26) & 0x3FFF;
                outHeight = readU16LE(data + 28) & 0x3FFF;
                outDetails = "Format: WebP (Lossy)";
                return true;
            }
        } else if (data[12] == 'V' && data[13] == 'P' && data[14] == '8' && data[15] == 'L') {
            if (size >= 25) {
                uint32_t b0 = data[21], b1 = data[22], b2 = data[23], b3 = data[24];
                outWidth = 1 + (((b1 & 0x3F) << 8) | b0);
                outHeight = 1 + (((b3 & 0x0F) << 10) | (b2 << 2) | ((b1 & 0xC0) >> 6));
                outDetails = "Format: WebP (Lossless)";
                return true;
            }
        }
    }

    // 6. SVG Header (Plain text / XML parse)
    if (ext == "svg") {
        std::string raw(reinterpret_cast<const char*>(data), std::min<std::size_t>(size, 4096));
        std::size_t svgTag = raw.find("<svg");
        if (svgTag != std::string::npos) {
            outDetails = "Format: SVG (Vector)";
            outWidth = 100;
            outHeight = 100; // Default placeholder for scalable vector
            return true;
        }
    }

    return false;
}

ExtractionResult ImageMetadataExtractor::extract(const std::string& filePath,
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
        result.errorDescription = "Image file not found";
        return result;
    }

    uint64_t fileSize = std::filesystem::file_size(fsPath, ec);
    result.originalFileSize = fileSize;

    // Read initial 32 KB header only (never loads entire multi-megabyte image)
    std::ifstream file(fsPath, std::ios::binary);
    if (!file.is_open()) {
        result.status = ExtractionStatus::PermissionDenied;
        result.errorDescription = "Cannot open image file";
        return result;
    }

    std::size_t readSize = static_cast<std::size_t>(std::min<uint64_t>(fileSize, 32 * 1024));
    std::vector<uint8_t> header(readSize);
    file.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(readSize));

    std::string ext = fsPath.extension().u8string();
    if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
    for (char& c : ext) c = static_cast<char>(std::tolower(c));

    uint32_t width = 0;
    uint32_t height = 0;
    std::string details;

    bool ok = extractDimensions(ext, header.data(), header.size(), width, height, details);

    if (ok) {
        std::ostringstream oss;
        oss << "Width: " << width << ", Height: " << height << ", " << details;
        result.metadataSummary = oss.str();
        result.status = ExtractionStatus::Success;

        // If SVG, check for <title> and <desc> text
        if (ext == "svg") {
            std::string svgText(reinterpret_cast<const char*>(header.data()), header.size());
            std::size_t tStartTag = svgText.find("<title>");
            if (tStartTag != std::string::npos) {
                std::size_t tEndTag = svgText.find("</title>", tStartTag);
                if (tEndTag != std::string::npos) {
                    result.extractedText = svgText.substr(tStartTag + 7, tEndTag - tStartTag - 7);
                    result.extractedByteCount = result.extractedText.size();
                }
            }
        }
    } else {
        result.status = ExtractionStatus::MetadataOnly;
        result.errorDescription = "Could not parse image header dimensions";
    }

    auto tEnd = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    return result;
}

} // namespace sse::extraction
