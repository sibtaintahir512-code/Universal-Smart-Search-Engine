#include "TestHarness.hpp"
#include "persistence/IndexSerializer.hpp"
#include "persistence/IndexDeserializer.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"

namespace sse::test {

void runPersistenceIntegrityTests() {
    std::cout << "[RUNNING] PersistenceIntegrity Tests..." << std::endl;

    using namespace sse::indexing;
    using namespace sse::persistence;

    // 1. Build a valid baseline serialized buffer
    DocumentCatalog catalog;
    InvertedIndex index;

    FileMetadata meta{};
    meta.fileSize = 100;
    DocID id = catalog.registerDocument(meta, "/test.txt", "test.txt");
    index.addTerm("sample", id, 1, 1, FIELD_FILENAME);
    index.documentLengthStatistics().recordDocumentLength(id, 1, 10);

    BinaryWriter validWriter;
    PersistenceResult serRes = IndexSerializer::serialize(catalog, index, validWriter, 1);
    TEST_ASSERT(serRes.isSuccess());

    dsa::CustomDynamicArray<uint8_t> validData = validWriter.buffer();
    TEST_ASSERT(validData.size() > 80);

    // 2. Truncated Buffer Tests
    {
        // Truncate to less than header
        DocumentCatalog cat;
        InvertedIndex idx;
        PersistenceResult res = IndexDeserializer::deserialize(validData.data(), 50, cat, idx);
        TEST_ASSERT(!res.isSuccess());
        TEST_ASSERT_EQ(static_cast<int>(res.status), static_cast<int>(PersistenceStatus::CorruptedHeader));

        // Truncate by 10 bytes at the end
        PersistenceResult res2 = IndexDeserializer::deserialize(validData.data(), validData.size() - 10, cat, idx);
        TEST_ASSERT(!res2.isSuccess());
    }

    // 3. Corrupted Trailing CRC32 Checksum
    {
        dsa::CustomDynamicArray<uint8_t> tampered = validData;
        tampered[tampered.size() - 1] ^= 0xFF; // Invert last checksum byte

        DocumentCatalog cat;
        InvertedIndex idx;
        PersistenceResult res = IndexDeserializer::deserialize(tampered.data(), tampered.size(), cat, idx);
        TEST_ASSERT(!res.isSuccess());
        TEST_ASSERT_EQ(static_cast<int>(res.status), static_cast<int>(PersistenceStatus::ChecksumMismatch));
    }

    // 4. Corrupted Section Payload Data
    {
        dsa::CustomDynamicArray<uint8_t> tampered = validData;
        if (tampered.size() > 350) {
            tampered[350] ^= 0x55;
            // Fix trailing file CRC so it gets past file-level check to test section CRC
            uint32_t newFileCrc = CRC32::calculate(tampered.data(), tampered.size() - 4);
            tampered[tampered.size() - 4] = static_cast<uint8_t>(newFileCrc & 0xFF);
            tampered[tampered.size() - 3] = static_cast<uint8_t>((newFileCrc >> 8) & 0xFF);
            tampered[tampered.size() - 2] = static_cast<uint8_t>((newFileCrc >> 16) & 0xFF);
            tampered[tampered.size() - 1] = static_cast<uint8_t>((newFileCrc >> 24) & 0xFF);

            DocumentCatalog cat;
            InvertedIndex idx;
            PersistenceResult res = IndexDeserializer::deserialize(tampered.data(), tampered.size(), cat, idx);
            TEST_ASSERT(!res.isSuccess());
            TEST_ASSERT_EQ(static_cast<int>(res.status), static_cast<int>(PersistenceStatus::ChecksumMismatch));
        }
    }

    // 5. Corrupted Section Offset (Points out of bounds)
    {
        dsa::CustomDynamicArray<uint8_t> tampered = validData;
        // Section descriptor 1 starts at offset 80. Offset field is at byte 80 + 8 = 88.
        tampered[88] = 0xFF;
        tampered[89] = 0xFF;
        tampered[90] = 0xFF;

        uint32_t newFileCrc = CRC32::calculate(tampered.data(), tampered.size() - 4);
        tampered[tampered.size() - 4] = static_cast<uint8_t>(newFileCrc & 0xFF);
        tampered[tampered.size() - 3] = static_cast<uint8_t>((newFileCrc >> 8) & 0xFF);
        tampered[tampered.size() - 2] = static_cast<uint8_t>((newFileCrc >> 16) & 0xFF);
        tampered[tampered.size() - 1] = static_cast<uint8_t>((newFileCrc >> 24) & 0xFF);

        DocumentCatalog cat;
        InvertedIndex idx;
        PersistenceResult res = IndexDeserializer::deserialize(tampered.data(), tampered.size(), cat, idx);
        TEST_ASSERT(!res.isSuccess());
        TEST_ASSERT_EQ(static_cast<int>(res.status), static_cast<int>(PersistenceStatus::CorruptedSection));
    }
}

} // namespace sse::test
