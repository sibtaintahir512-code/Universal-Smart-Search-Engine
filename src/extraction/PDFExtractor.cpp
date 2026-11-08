#include "extraction/PDFExtractor.hpp"
#include "extraction/PDFDocumentParser.hpp"
#include <fpdfview.h>
#include <fpdf_text.h>
#include <fpdf_doc.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <vector>
#include <mutex>

namespace sse::extraction {

namespace {

// Helper to append a Unicode codepoint to a UTF-8 string
void appendCodePoint(uint32_t cp, std::string& out) {
    if (cp <= 0x7F) {
        if (cp != 0) out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp >= 0xD800 && cp <= 0xDFFF) {
        out.append("\xEF\xBF\xBD"); // Unpaired surrogate
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

// Global thread-safe initialization of PDFium
struct PDFiumManager {
    PDFiumManager() {
        FPDF_LIBRARY_CONFIG config;
        config.version = 2;
        config.m_pUserFontPaths = nullptr;
        config.m_pIsolate = nullptr;
        config.m_v8EmbedderSlot = 0;
        FPDF_InitLibraryWithConfig(&config);
    }
    ~PDFiumManager() {
        FPDF_DestroyLibrary();
    }
};

void initPdfiumOnce() {
    static PDFiumManager s_manager;
}

} // anonymous namespace

bool PDFExtractor::supportsExtension(const std::string& ext) const noexcept {
    return ext == "pdf";
}

bool PDFExtractor::supportsCategory(FileCategory category) const noexcept {
    return category == FileCategory::Documents;
}

std::string PDFExtractor::extractTextFromPdfStream(const std::string& streamData) {
    // Legacy helper for raw stream text operator inspection
    std::string text;
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
            } else if (c == 'T' && i + 1 < etPos && (streamData[i + 1] == '*' || streamData[i + 1] == 'd')) {
                if (!text.empty() && text.back() != '\n') {
                    text.push_back('\n');
                }
                i += 2;
                continue;
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

    ExtractionResult result;
    result.sourcePath = filePath;
    result.extractorId = EXTRACTOR_ID;
    result.extractorVersion = "1.2.0";

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
        result.errorDescription = "PDF file too small or truncated";
        return result;
    }

    // Quick signature check
    {
        std::ifstream f(fsPath, std::ios::binary);
        char magic[5] = {0};
        f.read(magic, 5);
        if (std::strncmp(magic, "%PDF-", 5) != 0) {
            result.status = ExtractionStatus::Corrupted;
            result.errorDescription = "Missing %PDF- header magic bytes";
            return result;
        }
    }

    initPdfiumOnce();

    FPDF_DOCUMENT doc = FPDF_LoadDocument(filePath.c_str(), nullptr);
    if (!doc) {
        unsigned long err = FPDF_GetLastError();
        if (err == FPDF_ERR_PASSWORD) {
            result.status = ExtractionStatus::Encrypted;
            result.errorDescription = "PDF document is password-protected or encrypted";
            return result;
        } else if (err == FPDF_ERR_FILE) {
            result.status = ExtractionStatus::Failed;
            result.errorDescription = "Cannot open PDF file";
            return result;
        }

        // For non-standard, truncated, or pseudo-PDFs (e.g. from legacy unit tests without xref),
        // invoke custom fallback parser to maintain backward compatibility.
        PDFDocumentParser fallback;
        ExtractionResult fb = fallback.parseAndExtract(filePath, maxBytes, cancelToken);
        if (fb.status != ExtractionStatus::Failed) {
            return fb;
        }

        result.status = ExtractionStatus::Corrupted;
        result.errorDescription = "Invalid or corrupted PDF format";
        return result;
    }

    int pageCount = FPDF_GetPageCount(doc);
    result.metadataSummary = "EstimatedPages: " + std::to_string(pageCount > 0 ? pageCount : 1);

    std::string accumulatedText;
    bool isTruncated = false;

    for (int p = 0; p < pageCount; ++p) {
        if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
            FPDF_CloseDocument(doc);
            result.status = ExtractionStatus::Cancelled;
            result.errorDescription = "Extraction cancelled by user";
            return result;
        }

        if (accumulatedText.size() >= maxBytes) {
            isTruncated = true;
            break;
        }

        FPDF_PAGE page = FPDF_LoadPage(doc, p);
        if (!page) continue;

        FPDF_TEXTPAGE textPage = FPDFText_LoadPage(page);
        if (textPage) {
            int charCount = FPDFText_CountChars(textPage);
            if (charCount > 0) {
                std::vector<unsigned short> u16Buf(static_cast<std::size_t>(charCount + 1), 0);
                int written = FPDFText_GetText(textPage, 0, charCount, u16Buf.data());

                if (written > 0) {
                    std::string pageText;
                    pageText.reserve(static_cast<std::size_t>(written));

                    for (int i = 0; i < written && u16Buf[i] != 0; ++i) {
                        uint16_t u1 = u16Buf[i];
                        if (u1 >= 0xD800 && u1 <= 0xDBFF && i + 1 < written) {
                            uint16_t u2 = u16Buf[i + 1];
                            if (u2 >= 0xDC00 && u2 <= 0xDFFF) {
                                uint32_t cp = 0x10000 + ((static_cast<uint32_t>(u1 - 0xD800) << 10) | (u2 - 0xDC00));
                                appendCodePoint(cp, pageText);
                                ++i;
                                continue;
                            }
                        }
                        appendCodePoint(u1, pageText);
                    }

                    if (!pageText.empty()) {
                        if (!accumulatedText.empty() && accumulatedText.back() != '\n') {
                            accumulatedText.push_back('\n');
                        }

                        // Enforce output limit during accumulation
                        if (accumulatedText.size() + pageText.size() > maxBytes) {
                            std::size_t remaining = static_cast<std::size_t>(maxBytes - accumulatedText.size());
                            accumulatedText.append(pageText.data(), remaining);
                            isTruncated = true;
                            FPDFText_ClosePage(textPage);
                            FPDF_ClosePage(page);
                            break;
                        } else {
                            accumulatedText += pageText;
                        }
                    }
                }
            }
            FPDFText_ClosePage(textPage);
        }
        FPDF_ClosePage(page);
    }

    FPDF_CloseDocument(doc);

    if (accumulatedText.empty()) {
        // If PDFium found no extractable text (e.g. non-conforming synthetic PDF lacking font descriptors or scanned image),
        // query fallback parser to recover any plain text streams.
        PDFDocumentParser fallback;
        ExtractionResult fb = fallback.parseAndExtract(filePath, maxBytes, cancelToken);
        if (fb.isSuccess()) {
            return fb;
        }

        result.status = ExtractionStatus::MetadataOnly;
        result.errorDescription = "PDF contains no extractable text content streams (scanned or image-only)";
    } else {
        result.extractedText = std::move(accumulatedText);
        result.extractedByteCount = result.extractedText.size();
        result.isTruncated = isTruncated;
        result.status = isTruncated ? ExtractionStatus::Partial : ExtractionStatus::Success;
    }

    auto tEnd = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    return result;
}

} // namespace sse::extraction
