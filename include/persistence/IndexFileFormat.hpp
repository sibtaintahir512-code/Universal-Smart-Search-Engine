#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include "persistence/BinaryIO.hpp"
#include "persistence/CRC32.hpp"

namespace sse::persistence {

// Master Format Constants
constexpr uint32_t INDEX_MAGIC_SIGNATURE    = 0x534D5254; // ASCII "SMRT"
constexpr uint16_t INDEX_MAJOR_VERSION      = 1;
constexpr uint16_t INDEX_MINOR_VERSION      = 0;
constexpr uint32_t INDEX_ENDIANNESS_MARKER  = 0x01020304;
constexpr uint32_t INDEX_HEADER_SIZE        = 80;

// Maximum Safe Resource Limits (Untrusted input defense)
constexpr uint64_t MAX_INDEX_FILE_SIZE      = 2ULL * 1024 * 1024 * 1024; // 2 GB
constexpr uint64_t MAX_SECTION_SIZE         = 1ULL * 1024 * 1024 * 1024; // 1 GB
constexpr uint32_t MAX_SECTION_COUNT        = 64;
constexpr uint32_t MAX_DOCUMENT_COUNT       = 10'000'000;
constexpr uint32_t MAX_VOCABULARY_COUNT     = 10'000'000;
constexpr uint64_t MAX_POSTINGS_COUNT       = 1'000'000'000;
constexpr uint32_t MAX_STRING_LENGTH        = 65536;

enum class SectionType : uint16_t {
    Unknown              = 0,
    TokenizerInfo        = 1,
    DocumentCatalog      = 2,
    DocumentLengths      = 3,
    VocabularyDictionary = 4,
    PostingsBlock        = 5
};

constexpr uint32_t SECTION_FLAG_REQUIRED    = 0x0001;
constexpr uint32_t SECTION_FLAG_COMPRESSED  = 0x0002;

/**
 * @brief Master Index File Header (Explicitly serialized 80 bytes).
 */
struct IndexFileHeader {
    uint32_t magicSignature{INDEX_MAGIC_SIGNATURE};
    uint16_t majorVersion{INDEX_MAJOR_VERSION};
    uint16_t minorVersion{INDEX_MINOR_VERSION};
    uint32_t headerSize{INDEX_HEADER_SIZE};
    uint32_t endiannessMarker{INDEX_ENDIANNESS_MARKER};
    uint32_t featureFlags{0};
    uint32_t sectionCount{0};
    uint64_t sectionDirectoryOffset{INDEX_HEADER_SIZE};
    uint64_t generation{1};
    uint64_t timestamp{0};
    uint64_t totalFileSize{0};
    uint32_t totalDocuments{0};
    uint32_t activeDocuments{0};
    uint32_t totalVocabulary{0};
    uint32_t totalPostings{0};
    uint32_t headerChecksum{0}; // CRC32 of preceding 72 bytes
    uint32_t reserved{0};

    void serialize(BinaryWriter& writer) const {
        BinaryWriter tempWriter(INDEX_HEADER_SIZE);
        tempWriter.writeUint32(magicSignature);
        tempWriter.writeUint16(majorVersion);
        tempWriter.writeUint16(minorVersion);
        tempWriter.writeUint32(headerSize);
        tempWriter.writeUint32(endiannessMarker);
        tempWriter.writeUint32(featureFlags);
        tempWriter.writeUint32(sectionCount);
        tempWriter.writeUint64(sectionDirectoryOffset);
        tempWriter.writeUint64(generation);
        tempWriter.writeUint64(timestamp);
        tempWriter.writeUint64(totalFileSize);
        tempWriter.writeUint32(totalDocuments);
        tempWriter.writeUint32(activeDocuments);
        tempWriter.writeUint32(totalVocabulary);
        tempWriter.writeUint32(totalPostings);

        // Compute checksum over first 72 bytes
        uint32_t crc = CRC32::calculate(tempWriter.data(), tempWriter.size());
        tempWriter.writeUint32(crc);
        tempWriter.writeUint32(reserved);

        writer.writeBytes(tempWriter.data(), tempWriter.size());
    }

    bool deserialize(BinaryReader& reader) {
        if (reader.remaining() < INDEX_HEADER_SIZE) {
            return false;
        }

        const uint8_t* rawData = reader.currentPtr();

        if (!reader.readUint32(magicSignature) ||
            !reader.readUint16(majorVersion) ||
            !reader.readUint16(minorVersion) ||
            !reader.readUint32(headerSize) ||
            !reader.readUint32(endiannessMarker) ||
            !reader.readUint32(featureFlags) ||
            !reader.readUint32(sectionCount) ||
            !reader.readUint64(sectionDirectoryOffset) ||
            !reader.readUint64(generation) ||
            !reader.readUint64(timestamp) ||
            !reader.readUint64(totalFileSize) ||
            !reader.readUint32(totalDocuments) ||
            !reader.readUint32(activeDocuments) ||
            !reader.readUint32(totalVocabulary) ||
            !reader.readUint32(totalPostings) ||
            !reader.readUint32(headerChecksum) ||
            !reader.readUint32(reserved)) {
            return false;
        }

        // Validate basic integrity
        if (magicSignature != INDEX_MAGIC_SIGNATURE) {
            return false;
        }
        if (majorVersion != INDEX_MAJOR_VERSION) {
            return false;
        }
        if (endiannessMarker != INDEX_ENDIANNESS_MARKER) {
            return false;
        }
        if (headerSize != INDEX_HEADER_SIZE) {
            return false;
        }

        // Verify header CRC32 over the 72 bytes before the checksum field
        uint32_t computedCrc = CRC32::calculate(rawData, 72);
        if (computedCrc != headerChecksum) {
            return false;
        }

        return true;
    }
};

/**
 * @brief Section Directory Descriptor (Explicitly serialized 48 bytes).
 */
struct IndexSectionDescriptor {
    SectionType sectionType{SectionType::Unknown};
    uint16_t    sectionVersion{1};
    uint32_t    flags{SECTION_FLAG_REQUIRED};
    uint64_t    offset{0};
    uint64_t    storedSize{0};
    uint64_t    uncompressedSize{0};
    uint64_t    entryCount{0};
    uint32_t    checksum{0};
    uint32_t    reserved{0};

    void serialize(BinaryWriter& writer) const {
        writer.writeUint16(static_cast<uint16_t>(sectionType));
        writer.writeUint16(sectionVersion);
        writer.writeUint32(flags);
        writer.writeUint64(offset);
        writer.writeUint64(storedSize);
        writer.writeUint64(uncompressedSize);
        writer.writeUint64(entryCount);
        writer.writeUint32(checksum);
        writer.writeUint32(reserved);
    }

    bool deserialize(BinaryReader& reader) {
        uint16_t st = 0;
        if (!reader.readUint16(st) ||
            !reader.readUint16(sectionVersion) ||
            !reader.readUint32(flags) ||
            !reader.readUint64(offset) ||
            !reader.readUint64(storedSize) ||
            !reader.readUint64(uncompressedSize) ||
            !reader.readUint64(entryCount) ||
            !reader.readUint32(checksum) ||
            !reader.readUint32(reserved)) {
            return false;
        }
        sectionType = static_cast<SectionType>(st);
        return true;
    }

    bool isRequired() const noexcept {
        return (flags & SECTION_FLAG_REQUIRED) != 0;
    }
};

} // namespace sse::persistence
