#pragma once

#include "extraction/IContentExtractor.hpp"

namespace sse::extraction {

/**
 * @brief Extractor for Adobe Portable Document Format (PDF) files.
 * 
 * Supports:
 * - PDF stream text extraction (BT ... ET text blocks, Tj / TJ operators)
 * - FlateDecode zlib stream decompression
 * - Page counting and document metadata query (/Title, /Author, /CreationDate)
 * - Password/encryption detection (/Encrypt)
 * - Corruption and malformed trailer detection
 * - Anti-bomb expansion limits and cancellation polling
 */
class PDFExtractor : public IContentExtractor {
public:
    static constexpr const char* EXTRACTOR_ID = "PDFExtractor";
    static constexpr const char* EXTRACTOR_VERSION = "1.0.0";

    PDFExtractor() = default;
    ~PDFExtractor() override = default;

    bool supportsExtension(const std::string& extension) const noexcept override;
    bool supportsCategory(FileCategory category) const noexcept override;
    std::string getExtractorId() const noexcept override { return EXTRACTOR_ID; }
    std::string getExtractorVersion() const noexcept override { return EXTRACTOR_VERSION; }

    ExtractionResult extract(const std::string& filePath,
                            uint64_t maxBytes,
                            const std::atomic<bool>* cancelToken = nullptr) override;

    /**
     * @brief Parses raw PDF content streams to extract text between BT and ET operators.
     */
    static std::string extractTextFromPdfStream(const std::string& streamData);
};

} // namespace sse::extraction
