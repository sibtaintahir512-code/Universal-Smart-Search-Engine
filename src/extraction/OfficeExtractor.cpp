#include "extraction/OfficeExtractor.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <zlib.h>

namespace sse::extraction {

bool OfficeExtractor::supportsExtension(const std::string& ext) const noexcept {
    return (ext == "docx" || ext == "pptx" || ext == "xlsx");
}

bool OfficeExtractor::supportsCategory(FileCategory category) const noexcept {
    return category == FileCategory::Documents;
}

std::string OfficeExtractor::decodeXmlEntities(const std::string& text) {
    std::string result;
    result.reserve(text.size());

    std::size_t i = 0;
    const std::size_t n = text.size();

    while (i < n) {
        if (text[i] == '&') {
            std::size_t semi = text.find(';', i);
            if (semi != std::string::npos && semi - i <= 10) {
                std::string entity = text.substr(i + 1, semi - i - 1);
                if (entity == "amp") {
                    result.push_back('&');
                    i = semi + 1;
                    continue;
                } else if (entity == "lt") {
                    result.push_back('<');
                    i = semi + 1;
                    continue;
                } else if (entity == "gt") {
                    result.push_back('>');
                    i = semi + 1;
                    continue;
                } else if (entity == "quot") {
                    result.push_back('"');
                    i = semi + 1;
                    continue;
                } else if (entity == "apos") {
                    result.push_back('\'');
                    i = semi + 1;
                    continue;
                } else if (!entity.empty() && entity[0] == '#') {
                    // Numeric entity &#123; or &#x1F;
                    try {
                        uint32_t cp = 0;
                        if (entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X')) {
                            cp = static_cast<uint32_t>(std::stoul(entity.substr(2), nullptr, 16));
                        } else {
                            cp = static_cast<uint32_t>(std::stoul(entity.substr(1), nullptr, 10));
                        }
                        if (cp <= 0x7F) {
                            result.push_back(static_cast<char>(cp));
                        } else if (cp <= 0x7FF) {
                            result.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
                            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        } else if (cp <= 0xFFFF) {
                            result.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
                            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        } else {
                            result.append("\xEF\xBF\xBD");
                        }
                        i = semi + 1;
                        continue;
                    } catch (...) {}
                }
            }
        }
        result.push_back(text[i]);
        ++i;
    }

    return result;
}

std::string OfficeExtractor::stripXmlAndDecode(const std::string& xml) {
    std::string text;
    text.reserve(xml.size() / 2);

    bool insideTag = false;
    std::string currentTag;

    for (std::size_t i = 0; i < xml.size(); ++i) {
        char c = xml[i];

        if (c == '<') {
            insideTag = true;
            currentTag.clear();
        } else if (c == '>') {
            insideTag = false;
            // Paragraph breaks in OOXML
            if (currentTag == "w:p" || currentTag == "/w:p" ||
                currentTag == "a:p" || currentTag == "/a:p" ||
                currentTag == "row" || currentTag == "/row" ||
                currentTag == "w:br" || currentTag == "w:br/") {
                if (!text.empty() && text.back() != '\n') {
                    text.push_back('\n');
                }
            } else if (currentTag == "w:tab/" || currentTag == "w:tab") {
                text.push_back('\t');
            } else if (currentTag == "c" || currentTag == "/c") {
                if (!text.empty() && text.back() != ' ' && text.back() != '\n') {
                    text.push_back(' ');
                }
            }
        } else if (insideTag) {
            if (currentTag.size() < 16) {
                currentTag.push_back(c);
            }
        } else {
            text.push_back(c);
        }
    }

    return decodeXmlEntities(text);
}

namespace {
    // Structure representing a parsed ZIP Central Directory record
    struct ZipFileEntry {
        std::string filename;
        uint16_t    compressionMethod{0};
        uint32_t    compressedSize{0};
        uint32_t    uncompressedSize{0};
        uint32_t    localHeaderOffset{0};
    };

    // Helper to read unaligned uint16
    uint16_t readU16(const uint8_t* p) {
        return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
    }

    // Helper to read unaligned uint32
    uint32_t readU32(const uint8_t* p) {
        return static_cast<uint32_t>(p[0]) |
               (static_cast<uint32_t>(p[1]) << 8) |
               (static_cast<uint32_t>(p[2]) << 16) |
               (static_cast<uint32_t>(p[3]) << 24);
    }

    // Decompress raw deflate data using zlib
    bool decompressDeflate(const uint8_t* src, std::size_t srcLen,
                           std::vector<char>& dst, std::size_t expectedSize) {
        dst.resize(expectedSize);
        z_stream strm;
        std::memset(&strm, 0, sizeof(strm));

        // -MAX_WBITS specifies raw deflate without zlib/gzip headers
        if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) {
            return false;
        }

        strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(src));
        strm.avail_in = static_cast<uInt>(srcLen);
        strm.next_out = reinterpret_cast<Bytef*>(dst.data());
        strm.avail_out = static_cast<uInt>(dst.size());

        int ret = inflate(&strm, Z_FINISH);
        inflateEnd(&strm);

        return (ret == Z_STREAM_END || ret == Z_OK);
    }
}

ExtractionResult OfficeExtractor::extract(const std::string& filePath,
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
        result.errorDescription = "File not found";
        return result;
    }

    uint64_t fileSize = std::filesystem::file_size(fsPath, ec);
    if (ec) {
        result.status = ExtractionStatus::PermissionDenied;
        result.errorDescription = "Cannot access file size";
        return result;
    }
    result.originalFileSize = fileSize;

    if (fileSize < 22) { // Minimal ZIP size is 22 bytes (empty EOCD)
        result.status = ExtractionStatus::Corrupted;
        result.errorDescription = "File too small to be a valid Office OOXML package";
        return result;
    }

    std::ifstream file(fsPath, std::ios::binary);
    if (!file.is_open()) {
        result.status = ExtractionStatus::PermissionDenied;
        result.errorDescription = "Failed to open file for read access";
        return result;
    }

    // Read full package into memory buffer up to safety limit (default 50 MB)
    std::vector<uint8_t> zipBuffer(static_cast<std::size_t>(fileSize));
    file.read(reinterpret_cast<char*>(zipBuffer.data()), static_cast<std::streamsize>(fileSize));
    if (file.gcount() != static_cast<std::streamsize>(fileSize)) {
        result.status = ExtractionStatus::Corrupted;
        result.errorDescription = "Truncated file read";
        return result;
    }

    // Check for OLE encrypted package header (D0 CF 11 E0 A1 B1 1A E1)
    if (zipBuffer.size() >= 8 &&
        zipBuffer[0] == 0xD0 && zipBuffer[1] == 0xCF && zipBuffer[2] == 0x11 && zipBuffer[3] == 0xE0) {
        result.status = ExtractionStatus::Encrypted;
        result.errorDescription = "Office document is password-protected or encrypted (OLE container)";
        return result;
    }

    // Check for standard ZIP signature PK\x03\x04
    if (zipBuffer[0] != 'P' || zipBuffer[1] != 'K' || zipBuffer[2] != 0x03 || zipBuffer[3] != 0x04) {
        result.status = ExtractionStatus::Corrupted;
        result.errorDescription = "Invalid ZIP magic bytes; not an OOXML container";
        return result;
    }

    // Locate End of Central Directory (EOCD: signature PK\x05\x06)
    int64_t eocdOffset = -1;
    const int64_t maxSearch = std::min<int64_t>(fileSize, 65535 + 22);
    for (int64_t i = fileSize - 22; i >= static_cast<int64_t>(fileSize - maxSearch); --i) {
        if (zipBuffer[i] == 'P' && zipBuffer[i + 1] == 'K' && zipBuffer[i + 2] == 0x05 && zipBuffer[i + 3] == 0x06) {
            eocdOffset = i;
            break;
        }
    }

    if (eocdOffset == -1) {
        result.status = ExtractionStatus::Corrupted;
        result.errorDescription = "ZIP End of Central Directory record not found";
        return result;
    }

    const uint8_t* eocd = zipBuffer.data() + eocdOffset;
    uint16_t numEntries = readU16(eocd + 10);
    uint32_t cdOffset = readU32(eocd + 16);

    if (cdOffset >= fileSize) {
        result.status = ExtractionStatus::Corrupted;
        result.errorDescription = "Corrupted ZIP Central Directory offset";
        return result;
    }

    // Parse Central Directory entries
    std::vector<ZipFileEntry> entries;
    const uint8_t* cdPtr = zipBuffer.data() + cdOffset;
    const uint8_t* cdEnd = zipBuffer.data() + eocdOffset;

    for (uint16_t i = 0; i < numEntries && cdPtr + 46 <= cdEnd; ++i) {
        if (cdPtr[0] != 'P' || cdPtr[1] != 'K' || cdPtr[2] != 0x01 || cdPtr[3] != 0x02) {
            break;
        }
        uint16_t method = readU16(cdPtr + 10);
        uint32_t compSize = readU32(cdPtr + 20);
        uint32_t uncompSize = readU32(cdPtr + 24);
        uint16_t nameLen = readU16(cdPtr + 28);
        uint16_t extraLen = readU16(cdPtr + 30);
        uint16_t commentLen = readU16(cdPtr + 32);
        uint32_t localHdr = readU32(cdPtr + 42);

        if (cdPtr + 46 + nameLen <= cdEnd) {
            std::string entryName(reinterpret_cast<const char*>(cdPtr + 46), nameLen);
            entries.push_back({entryName, method, compSize, uncompSize, localHdr});
        }
        cdPtr += 46 + nameLen + extraLen + commentLen;
    }

    // Determine target files based on extension
    std::string ext = fsPath.extension().u8string();
    if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
    for (char& c : ext) c = static_cast<char>(std::tolower(c));

    std::vector<std::string> targetPatterns;
    if (ext == "docx") {
        targetPatterns.push_back("word/document.xml");
    } else if (ext == "pptx") {
        // Collect slide XMLs
        for (const auto& entry : entries) {
            if (entry.filename.rfind("ppt/slides/slide", 0) == 0 &&
                entry.filename.find(".xml") != std::string::npos) {
                targetPatterns.push_back(entry.filename);
            }
        }
        std::sort(targetPatterns.begin(), targetPatterns.end());
    } else if (ext == "xlsx") {
        targetPatterns.push_back("xl/sharedStrings.xml");
        for (const auto& entry : entries) {
            if (entry.filename.rfind("xl/worksheets/sheet", 0) == 0 &&
                entry.filename.find(".xml") != std::string::npos) {
                targetPatterns.push_back(entry.filename);
            }
        }
    }

    std::string combinedExtractedText;

    for (const auto& targetName : targetPatterns) {
        if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
            result.status = ExtractionStatus::Cancelled;
            return result;
        }

        // Find entry in Central Directory
        const ZipFileEntry* targetEntry = nullptr;
        for (const auto& e : entries) {
            if (e.filename == targetName) {
                targetEntry = &e;
                break;
            }
        }

        if (!targetEntry) continue;

        // Anti-decompression bomb check: max 50 MB per XML file
        if (targetEntry->uncompressedSize > 50 * 1024 * 1024) {
            result.status = ExtractionStatus::FileTooLarge;
            result.errorDescription = "Internal XML payload exceeds safe expansion limit (Zip bomb prevention)";
            return result;
        }

        // Locate Local File Header
        uint32_t lfhOff = targetEntry->localHeaderOffset;
        if (lfhOff + 30 > fileSize) continue;

        const uint8_t* lfh = zipBuffer.data() + lfhOff;
        if (lfh[0] != 'P' || lfh[1] != 'K' || lfh[2] != 0x03 || lfh[3] != 0x04) {
            continue;
        }

        uint16_t lfhNameLen = readU16(lfh + 26);
        uint16_t lfhExtraLen = readU16(lfh + 28);
        uint32_t dataOffset = lfhOff + 30 + lfhNameLen + lfhExtraLen;

        if (dataOffset + targetEntry->compressedSize > fileSize) continue;

        const uint8_t* compressedData = zipBuffer.data() + dataOffset;
        std::vector<char> xmlData;

        if (targetEntry->compressionMethod == 0) { // Stored (no compression)
            xmlData.resize(targetEntry->uncompressedSize);
            std::memcpy(xmlData.data(), compressedData, targetEntry->uncompressedSize);
        } else if (targetEntry->compressionMethod == 8) { // Deflate
            if (!decompressDeflate(compressedData, targetEntry->compressedSize, xmlData, targetEntry->uncompressedSize)) {
                result.status = ExtractionStatus::Corrupted;
                result.errorDescription = "Failed to decompress XML stream from Office container";
                return result;
            }
        } else {
            result.status = ExtractionStatus::UnsupportedFormat;
            result.errorDescription = "Unsupported ZIP compression method";
            return result;
        }

        std::string xmlStr(xmlData.data(), xmlData.size());
        std::string plainText = stripXmlAndDecode(xmlStr);

        if (!combinedExtractedText.empty() && !plainText.empty()) {
            combinedExtractedText.push_back('\n');
        }
        combinedExtractedText += plainText;

        if (combinedExtractedText.size() >= maxBytes) {
            break;
        }
    }

    if (combinedExtractedText.empty() && targetPatterns.empty()) {
        result.status = ExtractionStatus::MetadataOnly;
        result.errorDescription = "No extractable text XML streams found in Office container";
    } else {
        bool truncated = (combinedExtractedText.size() > maxBytes);
        if (truncated) {
            combinedExtractedText.resize(static_cast<std::size_t>(maxBytes));
        }
        result.extractedText = std::move(combinedExtractedText);
        result.extractedByteCount = result.extractedText.size();
        result.isTruncated = truncated;
        result.status = truncated ? ExtractionStatus::Partial : ExtractionStatus::Success;
    }

    auto tEnd = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    return result;
}

} // namespace sse::extraction
