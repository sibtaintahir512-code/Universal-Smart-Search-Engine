#include "TestHarness.hpp"
#include "persistence/IndexFileFormat.hpp"

namespace sse::test {

void runIndexFileHeaderTests() {
    std::cout << "[RUNNING] IndexFileHeader Tests..." << std::endl;

    using namespace sse::persistence;

    // 1. Serialization and Deserialization Round-Trip
    {
        IndexFileHeader header;
        header.totalDocuments = 42;
        header.activeDocuments = 40;
        header.totalVocabulary = 1000;
        header.totalFileSize = 123456;
        header.generation = 5;
        header.timestamp = 1731170000000ULL;
        header.sectionCount = 5;
        header.sectionDirectoryOffset = 80;

        BinaryWriter writer;
        header.serialize(writer);

        TEST_ASSERT_EQ(writer.size(), INDEX_HEADER_SIZE);

        BinaryReader reader(writer.data(), writer.size());
        IndexFileHeader restored;
        TEST_ASSERT(restored.deserialize(reader));

        TEST_ASSERT_EQ(restored.magicSignature, INDEX_MAGIC_SIGNATURE);
        TEST_ASSERT_EQ(restored.majorVersion, INDEX_MAJOR_VERSION);
        TEST_ASSERT_EQ(restored.minorVersion, INDEX_MINOR_VERSION);
        TEST_ASSERT_EQ(restored.endiannessMarker, INDEX_ENDIANNESS_MARKER);
        TEST_ASSERT_EQ(restored.totalDocuments, 42U);
        TEST_ASSERT_EQ(restored.activeDocuments, 40U);
        TEST_ASSERT_EQ(restored.totalVocabulary, 1000U);
        TEST_ASSERT_EQ(restored.totalFileSize, 123456ULL);
        TEST_ASSERT_EQ(restored.generation, 5ULL);
        TEST_ASSERT_EQ(restored.timestamp, 1731170000000ULL);
        TEST_ASSERT_EQ(restored.sectionCount, 5U);
    }

    // 2. Corrupted Magic Signature Rejection
    {
        IndexFileHeader header;
        BinaryWriter writer;
        header.serialize(writer);

        // Corrupt magic signature
        dsa::CustomDynamicArray<uint8_t> corruptedData = writer.buffer();
        corruptedData[0] = 'X';

        BinaryReader reader(corruptedData.data(), corruptedData.size());
        IndexFileHeader restored;
        TEST_ASSERT(!restored.deserialize(reader));
    }

    // 3. Header CRC32 Checksum Verification & Tamper Detection
    {
        IndexFileHeader header;
        BinaryWriter writer;
        header.serialize(writer);

        // Tamper with generation field (inside first 72 bytes)
        dsa::CustomDynamicArray<uint8_t> corruptedData = writer.buffer();
        corruptedData[32] ^= 0xFF;

        BinaryReader reader(corruptedData.data(), corruptedData.size());
        IndexFileHeader restored;
        // Checksum mismatch must reject the header
        TEST_ASSERT(!restored.deserialize(reader));
    }

    // 4. Incompatible Major Version Rejection
    {
        IndexFileHeader header;
        header.majorVersion = 2; // Future unsupported version
        BinaryWriter writer;
        header.serialize(writer);

        BinaryReader reader(writer.data(), writer.size());
        IndexFileHeader restored;
        TEST_ASSERT(!restored.deserialize(reader));
    }
}

} // namespace sse::test
