#pragma once

#include <cstdint>
#include <string>

namespace sse {

/**
 * @brief Unique dense document identifier.
 * 0 is reserved as INVALID_DOC_ID.
 */
using DocID = uint32_t;
constexpr DocID INVALID_DOC_ID = 0;

/**
 * @brief High-level file categorization based on extension.
 */
enum class FileCategory : uint8_t {
    Documents = 0,
    SourceCode,
    Images,
    Videos,
    Audio,
    Archives,
    Other
};

/**
 * @brief Extraction status representing the depth of indexing achieved.
 */
enum class ExtractionStatus : uint8_t {
    Pending = 0,
    Extracted,
    MetadataOnly,
    Failed,
    Truncated
};

/**
 * @brief Field origin bitmask flags for terms.
 */
enum FieldMask : uint8_t {
    FIELD_NONE     = 0x00,
    FIELD_FILENAME = 0x01,
    FIELD_CONTENT  = 0x02
};

/**
 * @brief In-memory posting node for the inverted index.
 * Verified to be exactly 24 bytes under MSVC 2022 x64 alignment rules.
 */
struct PostingNode {
    DocID    docId;        // 4 bytes: Document identifier (1 .. N)
    uint16_t bodyTermFreq; // 2 bytes: Occurrences in body content
    uint16_t nameTermFreq; // 2 bytes: Occurrences in filename
    uint8_t  fieldMask;    // 1 byte:  Bitmask flags (0x01 = Filename, 0x02 = Content)
    uint8_t  padding[7];   // 7 bytes: Explicit compiler padding for 8-byte pointer alignment
    PostingNode* next;     // 8 bytes: Pointer to next posting node in sorted order
};

// Compile-time verification of MSVC x64 24-byte layout on 64-bit platforms
#if defined(_M_X64) || defined(__x86_64__) || defined(__aarch64__)
static_assert(sizeof(PostingNode) == 24, "PostingNode must be exactly 24 bytes on 64-bit architectures.");
#endif

/**
 * @brief Core filesystem metadata record stored in the DocumentCatalog.
 */
struct FileMetadata {
    DocID            docId{INVALID_DOC_ID};
    uint64_t         fileSize{0};
    uint64_t         creationTime{0};
    uint64_t         lastModifiedTime{0};
    FileCategory     category{FileCategory::Other};
    ExtractionStatus extractionState{ExtractionStatus::Pending};
    uint32_t         nameOffset{0};
    uint16_t         nameLength{0};
    uint32_t         pathOffset{0};
    uint16_t         pathLength{0};
    uint16_t         extensionLength{0};
    uint8_t          flags{0x01}; // Bit 0: is_active (default 1)
};

} // namespace sse
