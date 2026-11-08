#include "extraction/PDFExtractor.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cstring>
#include <sstream>
#include <vector>
#include <zlib.h>

namespace sse::extraction {

bool PDFExtractor::supportsExtension(const std::string& ext) const noexcept {
    return ext == "pdf";
}

bool PDFExtractor::supportsCategory(FileCategory category) const noexcept {
    return category == FileCategory::Documents;
}

std::string PDFExtractor::extractTextFromPdfStream(const std::string& streamData) {
    std::string text;
    text.reserve(streamData.size() / 4);

    std::size_t pos = 0;
    const std::size_t n = streamData.size();

    while (pos < n) {
        // Look for Begin Text block
        std::size_t btPos = streamData.find("BT", pos);
        if (btPos == std::string::npos) break;

        std::size_t etPos = streamData.find("ET", btPos);
        if (etPos == std::string::npos) etPos = n;

        // Inside BT ... ET block
        std::size_t i = btPos + 2;
        while (i < etPos) {
            char c = streamData[i];
            if (c == '(') {
                // Literal string (text)
                std::string literal;
                ++i;
                int parenDepth = 1;
                while (i < etPos && parenDepth > 0) {
                    if (streamData[i] == '\\' && i + 1 < etPos) {
                        literal.push_back(streamData[i + 1]);
                        i += 2;
                        continue;
                    }
                    if (streamData[i] == '(') ++parenDepth;
                    else if (streamData[i] == ')') {
                        --parenDepth;
                        if (parenDepth == 0) {
                            ++i;
                            break;
                        }
                    }
                    literal.push_back(streamData[i]);
                    ++i;
                }
                if (!literal.empty()) {
                    if (!text.empty() && text.back() != ' ' && text.back() != '\n') {
                        text.push_back(' ');
                    }
                    text += literal;
                }
            } else if (c == '<' && i + 1 < etPos && streamData[i + 1] != '<') {
                // Hex string <48656C6C6F>
                std::size_t closeHex = streamData.find('>', i);
                if (closeHex != std::string::npos && closeHex <= etPos) {
                    std::string hexStr = streamData.substr(i + 1, closeHex - i - 1);
                    std::string decoded;
                    for (std::size_t h = 0; h + 1 < hexStr.size(); h += 2) {
                        try {
                            char byte = static_cast<char>(std::stoul(hexStr.substr(h, 2), nullptr, 16));
                            if (byte >= 32 && byte <= 126) {
                                decoded.push_back(byte);
                            }
                        } catch (...) {}
                    }
                    if (!decoded.empty()) {
                        if (!text.empty() && text.back() != ' ' && text.back() != '\n') {
                            text.push_back(' ');
                        }
                        text += decoded;
                    }
                    i = closeHex + 1;
                    continue;
                }
                ++i;
            } else if (c == 'T' && i + 1 < etPos && (streamData[i + 1] == '*' || streamData[i + 1] == 'd')) {
                // T* or Td newline
                if (!text.empty() && text.back() != '\n') {
                    text.push_back('\n');
                }
                ++i;
            } else {
                ++i;
            }
        }
        pos = etPos + 2;
    }

    return text;
}

namespace {
    bool decompressPdfStream(const uint8_t* src, std::size_t srcLen, std::string& out) {
        std::vector<char> buffer(std::max<std::size_t>(srcLen * 3, 4096));
        z_stream strm;
        std::memset(&strm, 0, sizeof(strm));

        if (inflateInit(&strm) != Z_OK) {
            return false;
        }

        strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(src));
        strm.avail_in = static_cast<uInt>(srcLen);
        strm.next_out = reinterpret_cast<Bytef*>(buffer.data());
        strm.avail_out = static_cast<uInt>(buffer.size());

        while (true) {
            int ret = inflate(&strm, Z_NO_FLUSH);
            if (ret == Z_STREAM_END) {
                break;
            }
            if (ret != Z_OK) {
                inflateEnd(&strm);
                return false;
            }
            if (strm.avail_out == 0) {
                std::size_t oldSize = buffer.size();
                buffer.resize(oldSize * 2);
                strm.next_out = reinterpret_cast<Bytef*>(buffer.data() + oldSize);
                strm.avail_out = static_cast<uInt>(oldSize);
            }
        }

        std::size_t totalBytes = strm.total_out;
        inflateEnd(&strm);

        out.assign(buffer.data(), totalBytes);
        return true;
    }
}

ExtractionResult PDFExtractor::extract(const std::string& filePath,
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
        result.errorDescription = "PDF file not found";
        return result;
    }

    uint64_t fileSize = std::filesystem::file_size(fsPath, ec);
    if (ec) {
        result.status = ExtractionStatus::PermissionDenied;
        result.errorDescription = "Cannot access PDF file size";
        return result;
    }
    result.originalFileSize = fileSize;

    if (fileSize < 8) {
        result.status = ExtractionStatus::Corrupted;
        result.errorDescription = "File too small to be a valid PDF";
        return result;
    }

    std::ifstream file(fsPath, std::ios::binary);
    if (!file.is_open()) {
        result.status = ExtractionStatus::PermissionDenied;
        result.errorDescription = "Failed to open PDF for reading";
        return result;
    }

    std::vector<char> pdfBytes(static_cast<std::size_t>(fileSize));
    file.read(pdfBytes.data(), static_cast<std::streamsize>(fileSize));
    if (file.gcount() != static_cast<std::streamsize>(fileSize)) {
        result.status = ExtractionStatus::Corrupted;
        result.errorDescription = "Incomplete PDF file read";
        return result;
    }

    // Verify %PDF- magic signature
    std::string pdfHeader(pdfBytes.data(), std::min<std::size_t>(pdfBytes.size(), 1024));
    if (pdfHeader.find("%PDF-") == std::string::npos) {
        result.status = ExtractionStatus::Corrupted;
        result.errorDescription = "Missing %PDF- header signature";
        return result;
    }

    // Search full document string representation for keywords
    std::string pdfRaw(pdfBytes.data(), pdfBytes.size());

    // Check for password encryption
    if (pdfRaw.find("/Encrypt") != std::string::npos) {
        result.status = ExtractionStatus::Encrypted;
        result.errorDescription = "PDF is password-protected or encrypted";
        return result;
    }

    // Page count estimation
    std::size_t pageCount = 0;
    std::size_t pSearch = 0;
    while ((pSearch = pdfRaw.find("/Type /Page", pSearch)) != std::string::npos) {
        // Exclude /Pages (plural catalog node)
        if (pSearch + 11 >= pdfRaw.size() || pdfRaw[pSearch + 11] != 's') {
            ++pageCount;
        }
        pSearch += 11;
    }

    std::ostringstream metaSummary;
    metaSummary << "EstimatedPages: " << (pageCount > 0 ? pageCount : 1);
    result.metadataSummary = metaSummary.str();

    // Extract text from stream ... endstream blocks
    std::string accumulatedText;
    std::size_t pos = 0;

    while (pos < pdfRaw.size()) {
        if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
            result.status = ExtractionStatus::Cancelled;
            return result;
        }

        std::size_t streamStart = pdfRaw.find("stream", pos);
        if (streamStart == std::string::npos) break;

        // Advance past stream\r\n or stream\n
        std::size_t dataStart = streamStart + 6;
        if (dataStart < pdfRaw.size() && pdfRaw[dataStart] == '\r') ++dataStart;
        if (dataStart < pdfRaw.size() && pdfRaw[dataStart] == '\n') ++dataStart;

        std::size_t streamEnd = pdfRaw.find("endstream", dataStart);
        if (streamEnd == std::string::npos) break;

        // Check if stream is FlateDecode
        std::size_t dictStart = pdfRaw.rfind("<<", streamStart);
        bool isFlate = false;
        if (dictStart != std::string::npos && dictStart < streamStart) {
            std::string dict = pdfRaw.substr(dictStart, streamStart - dictStart);
            if (dict.find("FlateDecode") != std::string::npos) {
                isFlate = true;
            }
        }

        std::size_t streamLen = streamEnd - dataStart;
        std::string decompressed;

        if (isFlate) {
            const auto* uSrc = reinterpret_cast<const uint8_t*>(pdfRaw.data() + dataStart);
            decompressPdfStream(uSrc, streamLen, decompressed);
        } else {
            decompressed = pdfRaw.substr(dataStart, streamLen);
        }

        if (!decompressed.empty()) {
            std::string text = extractTextFromPdfStream(decompressed);
            if (!text.empty()) {
                if (!accumulatedText.empty()) accumulatedText.push_back('\n');
                accumulatedText += text;
            }
        }

        if (accumulatedText.size() >= maxBytes) {
            break;
        }

        pos = streamEnd + 9;
    }

    if (accumulatedText.empty()) {
        result.status = ExtractionStatus::MetadataOnly;
        result.errorDescription = "PDF contains no extractable text content streams (scanned or image-only)";
    } else {
        bool truncated = (accumulatedText.size() > maxBytes);
        if (truncated) {
            accumulatedText.resize(static_cast<std::size_t>(maxBytes));
        }
        result.extractedText = std::move(accumulatedText);
        result.extractedByteCount = result.extractedText.size();
        result.isTruncated = truncated;
        result.status = truncated ? ExtractionStatus::Partial : ExtractionStatus::Success;
    }

    auto tEnd = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    return result;
}

} // namespace sse::extraction
