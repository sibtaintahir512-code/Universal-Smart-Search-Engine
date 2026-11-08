#pragma once

#include <string>
#include <atomic>
#include "core/CommonTypes.hpp"
#include "extraction/ExtractionResult.hpp"

namespace sse::extraction {

/**
 * @brief Abstract polymorphic interface implemented by all format-specific extractors.
 */
class IContentExtractor {
public:
    virtual ~IContentExtractor() = default;

    /**
     * @brief Checks if the extractor supports the specified normalized (lowercase, no leading dot) extension.
     */
    virtual bool supportsExtension(const std::string& extension) const noexcept = 0;

    /**
     * @brief Checks if the extractor handles the specified high-level category.
     */
    virtual bool supportsCategory(FileCategory category) const noexcept = 0;

    /**
     * @brief Returns unique string identifier for the extractor (used in cache validation).
     */
    virtual std::string getExtractorId() const noexcept = 0;

    /**
     * @brief Returns semantic version string of the extractor.
     */
    virtual std::string getExtractorVersion() const noexcept = 0;

    /**
     * @brief Executes content or metadata extraction on the target file.
     * 
     * @param filePath Absolute or canonical path to target file.
     * @param maxBytes Upper limit on extracted byte count (streaming safety cap).
     * @param cancelToken Optional atomic cancellation flag.
     * @return Populated ExtractionResult object.
     */
    virtual ExtractionResult extract(const std::string& filePath,
                                    uint64_t maxBytes,
                                    const std::atomic<bool>* cancelToken = nullptr) = 0;
};

} // namespace sse::extraction
