#include "extraction/TextExtractor.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <vector>
#include <algorithm>

namespace sse::extraction {

bool TextExtractor::supportsExtension(const std::string& ext) const noexcept {
    return (ext == "txt" || ext == "md" || ext == "markdown" || ext == "text" || ext == "log" ||
            ext == "csv" || ext == "tsv" || ext == "ini" || ext == "cfg" || ext == "json" ||
            ext == "xml" || ext == "rtf");
}

bool TextExtractor::supportsCategory(FileCategory category) const noexcept {
    return category == FileCategory::Documents;
}

namespace {

// Helper to append a Unicode codepoint to a UTF-8 string with RFC 3629 bounds
void appendCodePointToUtf8(uint32_t cp, std::string& out) {
    if (cp <= 0x7F) {
        // Discard null bytes in text to prevent string truncation in C-APIs
        if (cp != 0) {
            out.push_back(static_cast<char>(cp));
        }
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp >= 0xD800 && cp <= 0xDFFF) {
        // Unpaired surrogate -> replacement character
        out.append("\xEF\xBF\xBD");
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

// Strict RFC 3629 UTF-8 validator and cleaner
std::string validateAndCleanUtf8(const uint8_t* udata, std::size_t size) {
    std::string result;
    result.reserve(size);

    std::size_t i = 0;
    while (i < size) {
        uint8_t b1 = udata[i];

        if (b1 <= 0x7F) {
            // ASCII (filter null byte)
            if (b1 != 0) {
                result.push_back(static_cast<char>(b1));
            }
            ++i;
        } else if (b1 >= 0xC2 && b1 <= 0xDF) {
            // 2-byte sequence (0xC0, 0xC1 are overlong and rejected)
            if (i + 1 < size) {
                uint8_t b2 = udata[i + 1];
                if ((b2 & 0xC0) == 0x80) {
                    result.push_back(static_cast<char>(b1));
                    result.push_back(static_cast<char>(b2));
                    i += 2;
                    continue;
                }
            }
            result.append("\xEF\xBF\xBD");
            ++i;
        } else if (b1 >= 0xE0 && b1 <= 0xEF) {
            // 3-byte sequence
            if (i + 2 < size) {
                uint8_t b2 = udata[i + 1];
                uint8_t b3 = udata[i + 2];
                if ((b2 & 0xC0) == 0x80 && (b3 & 0xC0) == 0x80) {
                    uint32_t cp = ((b1 & 0x0F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
                    // Check overlong and surrogate halves (0xD800 - 0xDFFF)
                    if (cp >= 0x0800 && (cp < 0xD800 || cp > 0xDFFF)) {
                        result.push_back(static_cast<char>(b1));
                        result.push_back(static_cast<char>(b2));
                        result.push_back(static_cast<char>(b3));
                        i += 3;
                        continue;
                    }
                }
            }
            result.append("\xEF\xBF\xBD");
            ++i;
        } else if (b1 >= 0xF0 && b1 <= 0xF4) {
            // 4-byte sequence
            if (i + 3 < size) {
                uint8_t b2 = udata[i + 1];
                uint8_t b3 = udata[i + 2];
                uint8_t b4 = udata[i + 3];
                if ((b2 & 0xC0) == 0x80 && (b3 & 0xC0) == 0x80 && (b4 & 0xC0) == 0x80) {
                    uint32_t cp = ((b1 & 0x07) << 18) | ((b2 & 0x3F) << 12) | ((b3 & 0x3F) << 6) | (b4 & 0x3F);
                    if (cp >= 0x10000 && cp <= 0x10FFFF) {
                        result.push_back(static_cast<char>(b1));
                        result.push_back(static_cast<char>(b2));
                        result.push_back(static_cast<char>(b3));
                        result.push_back(static_cast<char>(b4));
                        i += 4;
                        continue;
                    }
                }
            }
            result.append("\xEF\xBF\xBD");
            ++i;
        } else {
            // Illegal lead byte (0xC0, 0xC1, 0xF5..0xFF)
            result.append("\xEF\xBF\xBD");
            ++i;
        }
    }

    return result;
}

// Minimal safe RTF reader
std::string parseRtf(const char* data, std::size_t size) {
    std::string text;
    text.reserve(size / 2);

    std::size_t i = 0;
    while (i < size) {
        char c = data[i];
        if (c == '\\') {
            ++i;
            if (i >= size) break;
            if (data[i] == 'p' && i + 2 < size && data[i + 1] == 'a' && data[i + 2] == 'r') {
                text.push_back('\n');
                i += 3;
            } else if (data[i] == 't' && i + 2 < size && data[i + 1] == 'a' && data[i + 2] == 'b') {
                text.push_back('\t');
                i += 3;
            } else if (data[i] == '\'') {
                // Hex escaped character: \'hh
                if (i + 2 < size) {
                    try {
                        std::string hStr(data + i + 1, 2);
                        char hexChar = static_cast<char>(std::stoul(hStr, nullptr, 16));
                        text.push_back(hexChar);
                        i += 3;
                        continue;
                    } catch (...) {}
                }
                ++i;
            } else {
                // Skip control word letters
                while (i < size && std::isalpha(static_cast<unsigned char>(data[i]))) {
                    ++i;
                }
                // Skip optional space delimiter
                if (i < size && data[i] == ' ') ++i;
            }
        } else if (c == '{' || c == '}') {
            ++i; // Skip braces
        } else if (c == '\r' || c == '\n') {
            ++i; // Ignore raw newlines in RTF
        } else {
            text.push_back(c);
            ++i;
        }
    }
    return text;
}

} // anonymous namespace

std::string TextExtractor::decodeToUtf8(const char* data, std::size_t size) {
    if (!data || size == 0) {
        return "";
    }

    // Check for RTF signature: {\rtf
    if (size >= 5 && data[0] == '{' && data[1] == '\\' && data[2] == 'r' && data[3] == 't' && data[4] == 'f') {
        std::string rtfText = parseRtf(data, size);
        return validateAndCleanUtf8(reinterpret_cast<const uint8_t*>(rtfText.data()), rtfText.size());
    }

    const auto* udata = reinterpret_cast<const uint8_t*>(data);

    // 1. Check for UTF-8 BOM (EF BB BF) -> Strip and validate remainder
    if (size >= 3 && udata[0] == 0xEF && udata[1] == 0xBB && udata[2] == 0xBF) {
        return validateAndCleanUtf8(udata + 3, size - 3);
    }

    // 2. Check for UTF-16 Little Endian BOM (FF FE) with surrogate pair assembly
    if (size >= 2 && udata[0] == 0xFF && udata[1] == 0xFE) {
        std::string utf8;
        utf8.reserve(size);
        for (std::size_t i = 2; i + 1 < size; i += 2) {
            uint16_t u1 = static_cast<uint16_t>(udata[i] | (static_cast<uint16_t>(udata[i + 1]) << 8));
            if (u1 >= 0xD800 && u1 <= 0xDBFF && i + 3 < size) {
                // High surrogate followed by potential low surrogate
                uint16_t u2 = static_cast<uint16_t>(udata[i + 2] | (static_cast<uint16_t>(udata[i + 3]) << 8));
                if (u2 >= 0xDC00 && u2 <= 0xDFFF) {
                    uint32_t cp = 0x10000 + ((static_cast<uint32_t>(u1 - 0xD800) << 10) | (u2 - 0xDC00));
                    appendCodePointToUtf8(cp, utf8);
                    i += 2; // Extra 2 bytes consumed
                    continue;
                }
            }
            appendCodePointToUtf8(u1, utf8);
        }
        return utf8;
    }

    // 3. Check for UTF-16 Big Endian BOM (FE FF) with surrogate pair assembly
    if (size >= 2 && udata[0] == 0xFE && udata[1] == 0xFF) {
        std::string utf8;
        utf8.reserve(size);
        for (std::size_t i = 2; i + 1 < size; i += 2) {
            uint16_t u1 = static_cast<uint16_t>((static_cast<uint16_t>(udata[i]) << 8) | udata[i + 1]);
            if (u1 >= 0xD800 && u1 <= 0xDBFF && i + 3 < size) {
                uint16_t u2 = static_cast<uint16_t>((static_cast<uint16_t>(udata[i + 2]) << 8) | udata[i + 3]);
                if (u2 >= 0xDC00 && u2 <= 0xDFFF) {
                    uint32_t cp = 0x10000 + ((static_cast<uint32_t>(u1 - 0xD800) << 10) | (u2 - 0xDC00));
                    appendCodePointToUtf8(cp, utf8);
                    i += 2;
                    continue;
                }
            }
            appendCodePointToUtf8(u1, utf8);
        }
        return utf8;
    }

    // 4. Default: Standard UTF-8 with strict RFC 3629 validation
    return validateAndCleanUtf8(udata, size);
}

ExtractionResult TextExtractor::extract(const std::string& filePath,
                                       uint64_t maxBytes,
                                       const std::atomic<bool>* cancelToken) {
    auto tStart = std::chrono::steady_clock::now();

    ExtractionResult result;
    result.sourcePath = filePath;
    result.extractorId = EXTRACTOR_ID;
    result.extractorVersion = "1.1.0";

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
    constexpr std::size_t CHUNK_SIZE = 64 * 1024;
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
