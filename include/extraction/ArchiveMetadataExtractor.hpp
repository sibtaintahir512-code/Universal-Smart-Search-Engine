#pragma once

#include "extraction/IContentExtractor.hpp"

namespace sse::extraction {

/**
 * @brief Extractor for compressed archive containers (ZIP, 7Z, RAR, TAR, GZ).
 * 
 * Enforces Release 1 Invariant:
 * - Archive contents are NOT extracted to disk or recursively indexed.
 * - Extracts entry counts, uncompressed size estimates, and compression format.
 * - Enforces anti-bomb resource protection.
 */
class ArchiveMetadataExtractor : public IContentExtractor {
public:
    static constexpr const char* EXTRACTOR_ID = "ArchiveMetadataExtractor";
    static constexpr const char* EXTRACTOR_VERSION = "1.0.0";

    ArchiveMetadataExtractor() = default;
    ~ArchiveMetadataExtractor() override = default;

    bool supportsExtension(const std::string& extension) const noexcept override;
    bool supportsCategory(FileCategory category) const noexcept override;
    std::string getExtractorId() const noexcept override { return EXTRACTOR_ID; }
    std::string getExtractorVersion() const noexcept override { return EXTRACTOR_VERSION; }

    ExtractionResult extract(const std::string& filePath,
                            uint64_t maxBytes,
                            const std::atomic<bool>* cancelToken = nullptr) override;

    /**
     * @brief Parses archive container headers without expanding internal data.
     */
    static bool parseArchiveHeader(const std::string& ext,
                                   const uint8_t* data,
                                   std::size_t size,
                                   uint64_t fileSize,
                                   uint32_t& outEntryCount,
                                   uint64_t& outUncompressedSize,
                                   std::string& outDetails);
};

} // namespace sse::extraction
