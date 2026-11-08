#include "extraction/PDFDocumentParser.hpp"
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <zlib.h>

namespace sse::extraction {

namespace {

// Helper to decode a hex character
int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Convert a Unicode codepoint to UTF-8
void appendCodePoint(uint32_t cp, std::string& out) {
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
        out.append("\xEF\xBF\xBD");
    }
}

// Parse a PDF hex string <FEFF0048...> into UTF-8
std::string decodePdfHexString(const std::string& hex) {
    std::string bytes;
    bytes.reserve(hex.size() / 2);

    int high = -1;
    for (char c : hex) {
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        int v = hexVal(c);
        if (v < 0) continue;
        if (high < 0) {
            high = v;
        } else {
            bytes.push_back(static_cast<char>((high << 4) | v));
            high = -1;
        }
    }
    if (high >= 0) {
        bytes.push_back(static_cast<char>(high << 4));
    }

    if (bytes.size() >= 2 && static_cast<uint8_t>(bytes[0]) == 0xFE && static_cast<uint8_t>(bytes[1]) == 0xFF) {
        // UTF-16 BE with BOM
        std::string utf8;
        for (std::size_t i = 2; i + 1 < bytes.size(); i += 2) {
            uint16_t unit = static_cast<uint16_t>((static_cast<uint8_t>(bytes[i]) << 8) | static_cast<uint8_t>(bytes[i + 1]));
            appendCodePoint(unit, utf8);
        }
        return utf8;
    } else if (bytes.size() == 2 && static_cast<uint8_t>(bytes[0]) == 0x00) {
        // 2-byte ASCII/Latin codepoint in CMap (e.g. <0041> -> 'A')
        std::string utf8;
        appendCodePoint(static_cast<uint8_t>(bytes[1]), utf8);
        return utf8;
    } else if (bytes.size() == 2) {
        // 2-byte Unicode codepoint
        uint16_t unit = static_cast<uint16_t>((static_cast<uint8_t>(bytes[0]) << 8) | static_cast<uint8_t>(bytes[1]));
        std::string utf8;
        appendCodePoint(unit, utf8);
        return utf8;
    }

    return bytes;
}

// Parse a PDF literal string (text\) with escape handling
std::string decodePdfLiteralString(const std::string& raw) {
    if (raw.size() >= 2 && static_cast<uint8_t>(raw[0]) == 0xFE && static_cast<uint8_t>(raw[1]) == 0xFF) {
        std::string utf8;
        for (std::size_t i = 2; i + 1 < raw.size(); i += 2) {
            uint16_t unit = static_cast<uint16_t>((static_cast<uint8_t>(raw[i]) << 8) | static_cast<uint8_t>(raw[i + 1]));
            appendCodePoint(unit, utf8);
        }
        return utf8;
    }
    return raw;
}

} // anonymous namespace

bool PDFDocumentParser::CMap::parseFromStream(const std::string& cmapStream) {
    codeToUtf8.clear();
    std::size_t pos = 0;

    // Parse beginbfchar ... endbfchar
    while ((pos = cmapStream.find("beginbfchar", pos)) != std::string::npos) {
        pos += 11;
        std::size_t endbf = cmapStream.find("endbfchar", pos);
        if (endbf == std::string::npos) break;

        std::string block = cmapStream.substr(pos, endbf - pos);
        std::istringstream iss(block);
        std::string srcHex, dstHex;
        while (iss >> srcHex >> dstHex) {
            if (srcHex.front() == '<') srcHex = srcHex.substr(1, srcHex.find('>') - 1);
            if (dstHex.front() == '<') dstHex = dstHex.substr(1, dstHex.find('>') - 1);
            try {
                uint32_t srcCode = static_cast<uint32_t>(std::stoul(srcHex, nullptr, 16));
                std::string mapped = decodePdfHexString(dstHex);
                codeToUtf8[srcCode] = mapped;
            } catch (...) {}
        }
        pos = endbf + 9;
    }

    // Parse beginbfrange ... endbfrange
    pos = 0;
    while ((pos = cmapStream.find("beginbfrange", pos)) != std::string::npos) {
        pos += 12;
        std::size_t endbf = cmapStream.find("endbfrange", pos);
        if (endbf == std::string::npos) break;

        std::string block = cmapStream.substr(pos, endbf - pos);
        std::istringstream iss(block);
        std::string startHex, endHex, dstHex;
        while (iss >> startHex >> endHex >> dstHex) {
            if (startHex.front() == '<') startHex = startHex.substr(1, startHex.find('>') - 1);
            if (endHex.front() == '<') endHex = endHex.substr(1, endHex.find('>') - 1);
            if (dstHex.front() == '<') dstHex = dstHex.substr(1, dstHex.find('>') - 1);

            try {
                uint32_t startCode = static_cast<uint32_t>(std::stoul(startHex, nullptr, 16));
                uint32_t endCode = static_cast<uint32_t>(std::stoul(endHex, nullptr, 16));
                uint32_t dstStart = static_cast<uint32_t>(std::stoul(dstHex, nullptr, 16));

                for (uint32_t c = startCode; c <= endCode && c <= startCode + 1000; ++c) {
                    uint32_t targetCp = dstStart + (c - startCode);
                    std::string u8;
                    appendCodePoint(targetCp, u8);
                    codeToUtf8[c] = u8;
                }
            } catch (...) {}
        }
        pos = endbf + 10;
    }

    return !codeToUtf8.empty();
}

std::string PDFDocumentParser::CMap::mapString(const std::string& raw) const {
    if (codeToUtf8.empty()) return raw;

    std::string out;
    for (unsigned char c : raw) {
        auto it = codeToUtf8.find(static_cast<uint32_t>(c));
        if (it != codeToUtf8.end()) {
            out += it->second;
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

PDFDocumentParser::PDFDocumentParser()
    : m_limits(Limits{}) {}

PDFDocumentParser::PDFDocumentParser(Limits limits)
    : m_limits(limits) {}

PDFDocumentParser::~PDFDocumentParser() = default;

bool PDFDocumentParser::findTrailerAndXRef(std::ifstream& file, uint64_t fileSize,
                                          uint64_t& outXRefOffset, std::string& outTrailerDict) {
    // Read the last 4 KB to locate startxref and trailer
    uint64_t searchSize = std::min<uint64_t>(fileSize, 4096);
    file.seekg(fileSize - searchSize);
    std::vector<char> endBuf(static_cast<std::size_t>(searchSize));
    file.read(endBuf.data(), static_cast<std::streamsize>(searchSize));
    std::string endStr(endBuf.data(), endBuf.size());

    std::size_t sxPos = endStr.rfind("startxref");
    if (sxPos == std::string::npos) return false;

    std::size_t numStart = sxPos + 9;
    while (numStart < endStr.size() && std::isspace(static_cast<unsigned char>(endStr[numStart]))) {
        ++numStart;
    }
    std::size_t numEnd = numStart;
    while (numEnd < endStr.size() && std::isdigit(static_cast<unsigned char>(endStr[numEnd]))) {
        ++numEnd;
    }
    if (numStart >= numEnd) return false;

    try {
        outXRefOffset = std::stoull(endStr.substr(numStart, numEnd - numStart));
    } catch (...) {
        return false;
    }

    // Locate trailer dictionary
    std::size_t trPos = endStr.rfind("trailer", sxPos);
    if (trPos != std::string::npos) {
        std::size_t dictStart = endStr.find("<<", trPos);
        std::size_t dictEnd = endStr.find(">>", dictStart);
        if (dictStart != std::string::npos && dictEnd != std::string::npos && dictEnd > dictStart) {
            outTrailerDict = endStr.substr(dictStart, dictEnd - dictStart + 2);
        }
    }

    return (outXRefOffset < fileSize);
}

bool PDFDocumentParser::decompressFlate(const uint8_t* compressed, std::size_t compLen,
                                       std::string& outDecompressed) {
    if (!compressed || compLen == 0) return false;

    // Strict bomb protection: limit initial allocation
    std::size_t initialBuf = std::min<std::size_t>(compLen * 4, 64 * 1024);
    std::vector<char> buffer(std::max<std::size_t>(initialBuf, 4096));

    z_stream strm;
    std::memset(&strm, 0, sizeof(strm));
    if (inflateInit(&strm) != Z_OK) return false;

    strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressed));
    strm.avail_in = static_cast<uInt>(compLen);
    strm.next_out = reinterpret_cast<Bytef*>(buffer.data());
    strm.avail_out = static_cast<uInt>(buffer.size());

    while (true) {
        int ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_END) break;
        if (ret != Z_OK) {
            inflateEnd(&strm);
            return false;
        }

        if (strm.avail_out == 0) {
            std::size_t currentOut = strm.total_out;

            // Enforce decompression budget
            if (currentOut >= m_limits.maxDecompressedStreamBytes ||
                (m_cumulativeDecompressed + currentOut) >= m_limits.maxCumulativeBytes) {
                inflateEnd(&strm);
                return false; // Exceeded budget
            }

            // Enforce 100:1 compression ratio
            if (compLen > 0 && (currentOut / compLen) > m_limits.maxDecompressionRatio) {
                inflateEnd(&strm);
                return false; // Compression bomb!
            }

            std::size_t newSize = buffer.size() * 2;
            if (newSize > m_limits.maxDecompressedStreamBytes) {
                newSize = static_cast<std::size_t>(m_limits.maxDecompressedStreamBytes);
            }
            if (newSize <= buffer.size()) {
                inflateEnd(&strm);
                return false;
            }

            buffer.resize(newSize);
            strm.next_out = reinterpret_cast<Bytef*>(buffer.data() + currentOut);
            strm.avail_out = static_cast<uInt>(buffer.size() - currentOut);
        }
    }

    std::size_t totalOut = strm.total_out;
    inflateEnd(&strm);

    m_cumulativeDecompressed += totalOut;
    outDecompressed.assign(buffer.data(), totalOut);
    return true;
}

void PDFDocumentParser::extractTextFromOperators(const std::string& content,
                                                const std::unordered_map<std::string, CMap>& fontCMaps,
                                                std::string& outText,
                                                uint64_t maxBytes) {
    std::size_t pos = 0;
    const std::size_t n = content.size();
    std::string currentFont;

    while (pos < n && outText.size() < maxBytes) {
        std::size_t btPos = content.find("BT", pos);
        if (btPos == std::string::npos) break;

        std::size_t etPos = content.find("ET", btPos);
        if (etPos == std::string::npos) etPos = n;

        std::size_t i = btPos + 2;
        while (i < etPos && outText.size() < maxBytes) {
            char c = content[i];

            // 1. Literal string: (text)
            if (c == '(') {
                std::string literal;
                ++i;
                int depth = 1;
                while (i < etPos && depth > 0) {
                    if (content[i] == '\\' && i + 1 < etPos) {
                        literal.push_back(content[i + 1]);
                        i += 2;
                        continue;
                    }
                    if (content[i] == '(') ++depth;
                    else if (content[i] == ')') {
                        --depth;
                        if (depth == 0) {
                            ++i;
                            break;
                        }
                    }
                    literal.push_back(content[i]);
                    ++i;
                }

                std::string decoded = decodePdfLiteralString(literal);
                auto it = fontCMaps.find(currentFont);
                if (it == fontCMaps.end()) {
                    it = fontCMaps.find("Default");
                }
                if (it != fontCMaps.end()) {
                    decoded = it->second.mapString(decoded);
                }

                if (!decoded.empty()) {
                    if (!outText.empty() && outText.back() != ' ' && outText.back() != '\n') {
                        outText.push_back(' ');
                    }
                    outText += decoded;
                }
            }
            // 2. Hex string: <48656C6C6F>
            else if (c == '<' && i + 1 < etPos && content[i + 1] != '<') {
                std::size_t closeHex = content.find('>', i);
                if (closeHex != std::string::npos && closeHex <= etPos) {
                    std::string hexStr = content.substr(i + 1, closeHex - i - 1);
                    std::string decoded = decodePdfHexString(hexStr);
                    auto it = fontCMaps.find(currentFont);
                    if (it == fontCMaps.end()) {
                        it = fontCMaps.find("Default");
                    }
                    if (it != fontCMaps.end()) {
                        decoded = it->second.mapString(decoded);
                    }
                    if (!decoded.empty()) {
                        if (!outText.empty() && outText.back() != ' ' && outText.back() != '\n') {
                            outText.push_back(' ');
                        }
                        outText += decoded;
                    }
                    i = closeHex + 1;
                    continue;
                }
                ++i;
            }
            // 3. Text positioning & newlines (T*, Td, TD)
            else if (c == 'T' && i + 1 < etPos) {
                char nextC = content[i + 1];
                if (nextC == '*' || nextC == 'd' || nextC == 'D') {
                    if (!outText.empty() && outText.back() != '\n') {
                        outText.push_back('\n');
                    }
                    i += 2;
                    continue;
                } else if (nextC == 'f') { // Font selection: /F1 12 Tf
                    std::size_t slash = content.rfind('/', i);
                    if (slash != std::string::npos && i - slash < 32) {
                        std::size_t fEnd = content.find_first_of(" \t\r\n", slash);
                        if (fEnd != std::string::npos && fEnd < i) {
                            currentFont = content.substr(slash + 1, fEnd - slash - 1);
                        }
                    }
                    i += 2;
                    continue;
                }
                ++i;
            } else {
                ++i;
            }
        }

        pos = etPos + 2;
    }
}

ExtractionResult PDFDocumentParser::parseAndExtract(const std::string& filePath,
                                                    uint64_t maxOutputBytes,
                                                    const std::atomic<bool>* cancelToken) {
    ExtractionResult result;
    result.sourcePath = filePath;
    result.extractorId = "PDFExtractor";
    result.extractorVersion = "1.1.0";
    m_cumulativeDecompressed = 0;

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        result.status = ExtractionStatus::PermissionDenied;
        result.errorDescription = "Cannot open PDF file for reading";
        return result;
    }

    file.seekg(0, std::ios::end);
    uint64_t fileSize = static_cast<uint64_t>(file.tellg());
    result.originalFileSize = fileSize;

    if (fileSize < 8) {
        result.status = ExtractionStatus::Corrupted;
        result.errorDescription = "PDF file truncated";
        return result;
    }

    // Verify %PDF- header
    file.seekg(0);
    char hdr[8];
    file.read(hdr, 5);
    hdr[5] = '\0';
    if (std::strncmp(hdr, "%PDF-", 5) != 0) {
        result.status = ExtractionStatus::Corrupted;
        result.errorDescription = "Missing %PDF- header magic signature";
        return result;
    }

    // Locate trailer and check encryption
    uint64_t xrefOffset = 0;
    std::string trailerDict;
    findTrailerAndXRef(file, fileSize, xrefOffset, trailerDict);

    // Read first 64 KB and last 64 KB to inspect dictionary for /Encrypt
    std::string scanSlice;
    {
        uint64_t readHead = std::min<uint64_t>(fileSize, 64 * 1024);
        file.seekg(0);
        std::vector<char> hBuf(readHead);
        file.read(hBuf.data(), static_cast<std::streamsize>(readHead));
        scanSlice.append(hBuf.data(), hBuf.size());

        if (fileSize > 64 * 1024) {
            uint64_t readTail = std::min<uint64_t>(fileSize, 64 * 1024);
            file.seekg(fileSize - readTail);
            std::vector<char> tBuf(readTail);
            file.read(tBuf.data(), static_cast<std::streamsize>(readTail));
            scanSlice.append(tBuf.data(), tBuf.size());
        }
    }

    if (scanSlice.find("/Encrypt") != std::string::npos ||
        trailerDict.find("/Encrypt") != std::string::npos) {
        result.status = ExtractionStatus::Encrypted;
        result.errorDescription = "PDF document is encrypted or password-protected";
        return result;
    }

    // Estimate Page Count
    std::size_t pageCount = 0;
    std::size_t pFind = 0;
    while ((pFind = scanSlice.find("/Type /Page", pFind)) != std::string::npos) {
        if (pFind + 11 >= scanSlice.size() || scanSlice[pFind + 11] != 's') {
            ++pageCount;
        }
        pFind += 11;
    }

    result.metadataSummary = "EstimatedPages: " + std::to_string(pageCount > 0 ? pageCount : 1);

    // Stream-based object extraction: scan objects by 64KB blocks without buffering entire PDF
    std::string accumulatedText;
    std::unordered_map<std::string, CMap> fontCMaps;

    file.seekg(0);
    constexpr std::size_t BUFFER_CHUNK = 64 * 1024;
    std::vector<char> chunk(BUFFER_CHUNK + 1024);
    uint64_t filePos = 0;

    while (filePos < fileSize && accumulatedText.size() < maxOutputBytes) {
        if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
            result.status = ExtractionStatus::Cancelled;
            result.errorDescription = "Extraction cancelled";
            return result;
        }

        file.seekg(filePos);
        uint64_t toRead = std::min<uint64_t>(fileSize - filePos, BUFFER_CHUNK);
        file.read(chunk.data(), static_cast<std::streamsize>(toRead));
        std::streamsize nRead = file.gcount();
        if (nRead <= 0) break;

        std::string block(chunk.data(), static_cast<std::size_t>(nRead));

        // Search for stream ... endstream within current block
        std::size_t sPos = 0;
        while ((sPos = block.find("stream", sPos)) != std::string::npos && accumulatedText.size() < maxOutputBytes) {
            // Check dictionary preceding this stream for FlateDecode
            bool isFlate = false;
            std::size_t dStart = block.rfind("<<", sPos);
            if (dStart != std::string::npos && sPos - dStart < 1024) {
                std::string dict = block.substr(dStart, sPos - dStart);
                if (dict.find("FlateDecode") != std::string::npos) {
                    isFlate = true;
                }
            }

            // Locate start of binary data (skip stream\r\n or stream\n)
            std::size_t dataStart = sPos + 6;
            if (dataStart < block.size() && block[dataStart] == '\r') ++dataStart;
            if (dataStart < block.size() && block[dataStart] == '\n') ++dataStart;

            uint64_t dataFileOffset = filePos + dataStart;

            // Find endstream
            std::size_t ePos = block.find("endstream", dataStart);
            std::size_t streamLen = 0;
            std::vector<uint8_t> rawStream;

            if (ePos != std::string::npos) {
                streamLen = ePos - dataStart;
                rawStream.assign(reinterpret_cast<const uint8_t*>(block.data() + dataStart),
                                 reinterpret_cast<const uint8_t*>(block.data() + dataStart + streamLen));
                sPos = ePos + 9;
            } else {
                // Stream extends past current block: read up to 1 MB maximum stream slice
                uint64_t maxStreamRead = std::min<uint64_t>(fileSize - dataFileOffset, 1024 * 1024);
                file.seekg(dataFileOffset);
                rawStream.resize(static_cast<std::size_t>(maxStreamRead));
                file.read(reinterpret_cast<char*>(rawStream.data()), static_cast<std::streamsize>(maxStreamRead));
                std::streamsize actualStream = file.gcount();
                rawStream.resize(static_cast<std::size_t>(actualStream));
                sPos += 6;
            }

            if (!rawStream.empty()) {
                std::string decompressed;
                if (isFlate) {
                    decompressFlate(rawStream.data(), rawStream.size(), decompressed);
                } else {
                    decompressed.assign(reinterpret_cast<const char*>(rawStream.data()), rawStream.size());
                }

                if (!decompressed.empty()) {
                    // Check if this stream is a CMap
                    if (decompressed.find("beginbfchar") != std::string::npos ||
                        decompressed.find("beginbfrange") != std::string::npos) {
                        CMap cmap;
                        if (cmap.parseFromStream(decompressed)) {
                            fontCMaps["Default"] = std::move(cmap);
                        }
                    } else if (decompressed.find("BT") != std::string::npos) {
                        extractTextFromOperators(decompressed, fontCMaps, accumulatedText, maxOutputBytes);
                    }
                }
            }
        }

        // Advance by chunk minus 512 bytes overlap to avoid splitting keywords
        filePos += (toRead > 512 ? toRead - 512 : toRead);
    }

    if (accumulatedText.empty()) {
        result.status = ExtractionStatus::MetadataOnly;
        result.errorDescription = "PDF contains no extractable text content streams (scanned or image-only)";
    } else {
        bool truncated = (accumulatedText.size() > maxOutputBytes);
        if (truncated) {
            accumulatedText.resize(static_cast<std::size_t>(maxOutputBytes));
        }
        result.extractedText = std::move(accumulatedText);
        result.extractedByteCount = result.extractedText.size();
        result.isTruncated = truncated;
        result.status = truncated ? ExtractionStatus::Partial : ExtractionStatus::Success;
    }

    return result;
}

} // namespace sse::extraction
