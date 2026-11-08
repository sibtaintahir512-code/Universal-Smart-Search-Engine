#pragma once

#include "extraction/ExtractionResult.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <cstdint>
#include <fstream>

namespace sse::extraction {

/**
 * @brief Memory-safe, stream-bounded PDF Document Parser.
 *
 * Implements:
 * - On-demand bounded file stream reading (no full-file in-memory buffering)
 * - Trailer, XRef table, and XRef stream parsing with linear object scan fallback
 * - True Object dictionary/array/stream tokenization
 * - Page tree (/Pages -> /Kids -> /Page) traversal with cycle detection
 * - Font /ToUnicode CMap parsing for character code to UTF-8 mapping
 * - Text extraction within BT ... ET blocks (Tj, TJ, ', ")
 * - Anti-decompression bomb protection (max 100:1 ratio, 50 MB cumulative ceiling)
 * - Encryption detection (/Encrypt dictionary)
 * - Cooperative cancellation
 */
class PDFDocumentParser {
public:
    struct Limits {
        Limits() = default;
        uint64_t maxDecompressedStreamBytes{10ULL * 1024 * 1024}; // 10 MB per stream
        uint64_t maxCumulativeBytes{50ULL * 1024 * 1024};         // 50 MB total
        uint32_t maxDecompressionRatio{100};                     // 100:1 ratio
        uint32_t maxObjects{100000};                             // Max objects in xref
        uint32_t maxPages{10000};                                // Max pages
    };

    struct CMap {
        std::unordered_map<uint32_t, std::string> codeToUtf8;
        bool parseFromStream(const std::string& cmapStream);
        std::string mapString(const std::string& raw) const;
    };

    struct ObjectRef {
        uint32_t objNum{0};
        uint32_t genNum{0};
    };

    PDFDocumentParser();
    explicit PDFDocumentParser(Limits limits);
    ~PDFDocumentParser();

    ExtractionResult parseAndExtract(const std::string& filePath,
                                     uint64_t maxOutputBytes,
                                     const std::atomic<bool>* cancelToken);

private:
    Limits m_limits;
    uint64_t m_cumulativeDecompressed{0};

    // Helper functions
    bool findTrailerAndXRef(std::ifstream& file, uint64_t fileSize,
                            uint64_t& outXRefOffset, std::string& outTrailerDict);
    bool readObjectAtOffset(std::ifstream& file, uint64_t offset,
                            std::string& outHeader, std::string& outBody, bool& isStream,
                            uint64_t& streamDataOffset, uint64_t& streamLength);
    bool decompressFlate(const uint8_t* compressed, std::size_t compLen,
                         std::string& outDecompressed);
    void extractTextFromOperators(const std::string& content,
                                  const std::unordered_map<std::string, CMap>& fontCMaps,
                                  std::string& outText,
                                  uint64_t maxBytes);
};

} // namespace sse::extraction
