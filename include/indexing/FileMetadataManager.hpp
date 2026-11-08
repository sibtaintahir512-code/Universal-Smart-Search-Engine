#pragma once

#include <cstdint>
#include <string>
#include <filesystem>
#include "core/CommonTypes.hpp"

namespace sse::indexing {

/**
 * @brief High-level file record holding full string paths and system metadata.
 * Used during ingestion before compact storage in DocumentCatalog.
 */
struct ExtendedFileRecord {
    std::string  filename;
    std::string  absolutePath;
    std::string  canonicalPath;
    std::string  extension;
    std::string  mimeType;
    FileMetadata metadata;
};

/**
 * @brief Subsystem responsible for extracting filesystem attributes,
 * classifying file formats by extension, and preparing metadata records.
 */
class FileMetadataManager {
public:
    FileMetadataManager() = default;
    ~FileMetadataManager() = default;

    /**
     * @brief Extracts metadata from a filesystem path without throwing exceptions.
     * @param path Target filesystem path.
     * @param outRecord Populated metadata record.
     * @return true if file exists and attributes were queried; false on error.
     */
    bool extractMetadata(const std::filesystem::path& path, ExtendedFileRecord& outRecord) const noexcept;

    /**
     * @brief Classifies file extension into approved FileCategory enum.
     */
    static FileCategory classifyExtension(const std::string& extension) noexcept;

    /**
     * @brief Heuristic MIME-type detection based on canonical extension.
     */
    static std::string detectMimeType(const std::string& extension) noexcept;

    /**
     * @brief Checks if file type is eligible for future text/content extraction (Phase 4).
     */
    static bool supportsContentExtraction(const std::string& extension) noexcept;

    /**
     * @brief Normalizes extension: strips leading dot and converts ASCII to lowercase.
     */
    static std::string normalizeExtension(const std::string& rawExt) noexcept;

    /**
     * @brief Converts filesystem file_time_type to 64-bit epoch seconds safely.
     */
    static uint64_t fileTimeToEpoch(const std::filesystem::file_time_type& ftime) noexcept;
};

} // namespace sse::indexing
