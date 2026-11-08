#pragma once

#include "extraction/IContentExtractor.hpp"
#include <vector>

namespace sse::extraction {

/**
 * @brief Extractor for Microsoft Office OpenXML documents (DOCX, PPTX, XLSX).
 * 
 * Implements:
 * - Direct ZIP container navigation and central directory parsing
 * - Decompression of internal XML payloads (word/document.xml, ppt/slides/slide*.xml, xl/sharedStrings.xml)
 * - Raw XML tag stripping with paragraph and cell boundary preservation
 * - Standard and numeric XML entity decoding
 * - Encryption and corruption detection
 * - Anti-zip bomb expansion limits
 */
class OfficeExtractor : public IContentExtractor {
public:
    static constexpr const char* EXTRACTOR_ID = "OfficeExtractor";
    static constexpr const char* EXTRACTOR_VERSION = "1.0.0";

    OfficeExtractor() = default;
    ~OfficeExtractor() override = default;

    bool supportsExtension(const std::string& extension) const noexcept override;
    bool supportsCategory(FileCategory category) const noexcept override;
    std::string getExtractorId() const noexcept override { return EXTRACTOR_ID; }
    std::string getExtractorVersion() const noexcept override { return EXTRACTOR_VERSION; }

    ExtractionResult extract(const std::string& filePath,
                            uint64_t maxBytes,
                            const std::atomic<bool>* cancelToken = nullptr) override;

    /**
     * @brief Parses an OOXML XML string, stripping tags and decoding XML entities.
     */
    static std::string stripXmlAndDecode(const std::string& xml);

    /**
     * @brief Decodes XML entities (&amp;, &lt;, &gt;, &quot;, &apos;, &#123;) to UTF-8.
     */
    static std::string decodeXmlEntities(const std::string& text);
};

} // namespace sse::extraction
