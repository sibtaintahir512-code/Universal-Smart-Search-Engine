#pragma once

#include "extraction/IContentExtractor.hpp"

namespace sse::extraction {

/**
 * @brief Extractor for plain text documents (TXT, MD) with multi-encoding BOM awareness.
 * 
 * Supports:
 * - UTF-8 (with or without BOM EF BB BF)
 * - UTF-16 Little Endian (BOM FF FE)
 * - UTF-16 Big Endian (BOM FE FF)
 * - Streaming extraction up to configurable maxBytes budget
 * - Cooperative cancellation polling
 */
class TextExtractor : public IContentExtractor {
public:
    static constexpr const char* EXTRACTOR_ID = "TextExtractor";
    static constexpr const char* EXTRACTOR_VERSION = "1.0.0";

    TextExtractor() = default;
    ~TextExtractor() override = default;

    bool supportsExtension(const std::string& extension) const noexcept override;
    bool supportsCategory(FileCategory category) const noexcept override;
    std::string getExtractorId() const noexcept override { return EXTRACTOR_ID; }
    std::string getExtractorVersion() const noexcept override { return EXTRACTOR_VERSION; }

    ExtractionResult extract(const std::string& filePath,
                            uint64_t maxBytes,
                            const std::atomic<bool>* cancelToken = nullptr) override;

    /**
     * @brief Decodes raw byte buffer into valid UTF-8 string based on detected BOM.
     */
    static std::string decodeToUtf8(const char* data, std::size_t size);
};

} // namespace sse::extraction
