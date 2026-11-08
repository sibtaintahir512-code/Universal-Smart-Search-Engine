#pragma once

#include "extraction/IContentExtractor.hpp"

namespace sse::extraction {

/**
 * @brief Extractor for audio and video media containers.
 * 
 * Extracts:
 * - Audio: Duration, sample rate, channels, bitrate, ID3v2 tags (Title, Artist)
 * - Video: Duration, resolution (width/height), container format
 * - Zero full-file multimedia decoding or transcription (fast container header scan)
 */
class MediaMetadataExtractor : public IContentExtractor {
public:
    static constexpr const char* EXTRACTOR_ID = "MediaMetadataExtractor";
    static constexpr const char* EXTRACTOR_VERSION = "1.0.0";

    MediaMetadataExtractor() = default;
    ~MediaMetadataExtractor() override = default;

    bool supportsExtension(const std::string& extension) const noexcept override;
    bool supportsCategory(FileCategory category) const noexcept override;
    std::string getExtractorId() const noexcept override { return EXTRACTOR_ID; }
    std::string getExtractorVersion() const noexcept override { return EXTRACTOR_VERSION; }

    ExtractionResult extract(const std::string& filePath,
                            uint64_t maxBytes,
                            const std::atomic<bool>* cancelToken = nullptr) override;

    /**
     * @brief Parses container headers to extract technical media parameters.
     */
    static bool parseMediaHeader(const std::string& ext,
                                 const uint8_t* data,
                                 std::size_t size,
                                 uint64_t fileSize,
                                 double& outDurationSec,
                                 uint32_t& outWidth,
                                 uint32_t& outHeight,
                                 uint32_t& outSampleRate,
                                 uint16_t& outChannels,
                                 std::string& outTags);
};

} // namespace sse::extraction
