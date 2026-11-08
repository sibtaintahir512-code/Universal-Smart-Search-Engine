#include "extraction/PDFExtractor.hpp"
#include "extraction/PDFDocumentParser.hpp"
#include <chrono>

namespace sse::extraction {

bool PDFExtractor::supportsExtension(const std::string& ext) const noexcept {
    return ext == "pdf";
}

bool PDFExtractor::supportsCategory(FileCategory category) const noexcept {
    return category == FileCategory::Documents;
}

std::string PDFExtractor::extractTextFromPdfStream(const std::string& streamData) {
    PDFDocumentParser parser;
    std::unordered_map<std::string, PDFDocumentParser::CMap> fontCMaps;
    std::string text;
    // Test helper for raw streams
    std::size_t pos = 0;
    const std::size_t n = streamData.size();

    while (pos < n) {
        std::size_t btPos = streamData.find("BT", pos);
        if (btPos == std::string::npos) break;

        std::size_t etPos = streamData.find("ET", btPos);
        if (etPos == std::string::npos) etPos = n;

        std::size_t i = btPos + 2;
        while (i < etPos) {
            char c = streamData[i];
            if (c == '(') {
                std::string literal;
                ++i;
                int depth = 1;
                while (i < etPos && depth > 0) {
                    if (streamData[i] == '\\' && i + 1 < etPos) {
                        literal.push_back(streamData[i + 1]);
                        i += 2;
                        continue;
                    }
                    if (streamData[i] == '(') ++depth;
                    else if (streamData[i] == ')') {
                        --depth;
                        if (depth == 0) {
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

ExtractionResult PDFExtractor::extract(const std::string& filePath,
                                      uint64_t maxBytes,
                                      const std::atomic<bool>* cancelToken) {
    auto tStart = std::chrono::steady_clock::now();

    PDFDocumentParser parser;
    ExtractionResult result = parser.parseAndExtract(filePath, maxBytes, cancelToken);
    result.extractorId = EXTRACTOR_ID;
    result.extractorVersion = EXTRACTOR_VERSION;

    auto tEnd = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    return result;
}

} // namespace sse::extraction
