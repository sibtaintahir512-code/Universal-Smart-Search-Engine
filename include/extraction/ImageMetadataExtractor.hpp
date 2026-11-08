#pragma once

#include "extraction/IContentExtractor.hpp"

namespace sse::extraction {

/**
 * @brief Extractor for image dimensions, format markers, and metadata.
 * 
 * Extracts:
 * - Width, height, bit depth without decoding pixel raster
 * - Format verification (PNG, JPEG, GIF, BMP, WEBP, SVG, TIFF)
 * - Untrusted SVG parsing with XXE entity prevention
 * - Zero OCR or full-image rendering (fast header-only extraction)
 */
class ImageMetadataExtractor : public IContentExtractor {
public:
    static constexpr const char* EXTRACTOR_ID = "ImageMetadataExtractor";
    static constexpr const char* EXTRACTOR_VERSION = "1.0.0";

    ImageMetadataExtractor() = default;
    ~ImageMetadataExtractor() override = default;

    bool supportsExtension(const std::string& extension) const noexcept override;
    bool supportsCategory(FileCategory category) const noexcept override;
    std::string getExtractorId() const noexcept override { return EXTRACTOR_ID; }
    std::string getExtractorVersion() const noexcept override { return EXTRACTOR_VERSION; }

    ExtractionResult extract(const std::string& filePath,
                            uint64_t maxBytes,
                            const std::atomic<bool>* cancelToken = nullptr) override;

    /**
     * @brief Parses dimensions directly from binary header bytes without decoding image.
     */
    static bool extractDimensions(const std::string& ext,
                                  const uint8_t* data,
                                  std::size_t size,
                                  uint32_t& outWidth,
                                  uint32_t& outHeight,
                                  std::string& outDetails);
};

} // namespace sse::extraction
