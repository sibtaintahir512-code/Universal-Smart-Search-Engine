#pragma once

#include "extraction/IContentExtractor.hpp"

namespace sse::extraction {

/**
 * @brief Extractor for programming source code and markup files.
 * 
 * Preserves line structure, whitespace indentation, and multi-encoding code files.
 * Supported formats: CPP, C, H, HPP, PY, JAVA, JS, TS, HTML, CSS, SQL, ASM, JSON, XML, CMAKE.
 */
class SourceCodeExtractor : public IContentExtractor {
public:
    static constexpr const char* EXTRACTOR_ID = "SourceCodeExtractor";
    static constexpr const char* EXTRACTOR_VERSION = "1.0.0";

    SourceCodeExtractor() = default;
    ~SourceCodeExtractor() override = default;

    bool supportsExtension(const std::string& extension) const noexcept override;
    bool supportsCategory(FileCategory category) const noexcept override;
    std::string getExtractorId() const noexcept override { return EXTRACTOR_ID; }
    std::string getExtractorVersion() const noexcept override { return EXTRACTOR_VERSION; }

    ExtractionResult extract(const std::string& filePath,
                            uint64_t maxBytes,
                            const std::atomic<bool>* cancelToken = nullptr) override;
};

} // namespace sse::extraction
