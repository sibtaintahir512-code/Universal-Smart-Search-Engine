#include "TestHarness.hpp"
#include "persistence/IndexFileFormat.hpp"

namespace sse::test {

void runSectionDirectoryTests() {
    std::cout << "[RUNNING] SectionDirectory Tests..." << std::endl;

    using namespace sse::persistence;

    // 1. Single Descriptor Round-Trip
    {
        IndexSectionDescriptor desc;
        desc.sectionType = SectionType::DocumentCatalog;
        desc.sectionVersion = 1;
        desc.flags = SECTION_FLAG_REQUIRED;
        desc.offset = 320;
        desc.storedSize = 1024;
        desc.uncompressedSize = 1024;
        desc.entryCount = 50;
        desc.checksum = 0xAABBCCDD;

        BinaryWriter writer;
        desc.serialize(writer);

        TEST_ASSERT_EQ(writer.size(), 48); // Descriptor is 48 bytes

        BinaryReader reader(writer.data(), writer.size());
        IndexSectionDescriptor restored;
        TEST_ASSERT(restored.deserialize(reader));

        TEST_ASSERT_EQ(static_cast<uint16_t>(restored.sectionType), static_cast<uint16_t>(SectionType::DocumentCatalog));
        TEST_ASSERT_EQ(restored.sectionVersion, 1);
        TEST_ASSERT(restored.isRequired());
        TEST_ASSERT_EQ(restored.offset, 320ULL);
        TEST_ASSERT_EQ(restored.storedSize, 1024ULL);
        TEST_ASSERT_EQ(restored.uncompressedSize, 1024ULL);
        TEST_ASSERT_EQ(restored.entryCount, 50ULL);
        TEST_ASSERT_EQ(restored.checksum, 0xAABBCCDDU);
    }

    // 2. Multi-Section Directory
    {
        BinaryWriter writer;
        IndexSectionDescriptor d1, d2, d3;
        d1.sectionType = SectionType::TokenizerInfo;
        d1.offset = 100;
        d2.sectionType = SectionType::VocabularyDictionary;
        d2.offset = 200;
        d3.sectionType = SectionType::PostingsBlock;
        d3.offset = 500;

        d1.serialize(writer);
        d2.serialize(writer);
        d3.serialize(writer);

        TEST_ASSERT_EQ(writer.size(), 48 * 3);

        BinaryReader reader(writer.data(), writer.size());
        IndexSectionDescriptor r1, r2, r3;
        TEST_ASSERT(r1.deserialize(reader));
        TEST_ASSERT(r2.deserialize(reader));
        TEST_ASSERT(r3.deserialize(reader));

        TEST_ASSERT_EQ(static_cast<uint16_t>(r1.sectionType), static_cast<uint16_t>(SectionType::TokenizerInfo));
        TEST_ASSERT_EQ(static_cast<uint16_t>(r2.sectionType), static_cast<uint16_t>(SectionType::VocabularyDictionary));
        TEST_ASSERT_EQ(static_cast<uint16_t>(r3.sectionType), static_cast<uint16_t>(SectionType::PostingsBlock));
        TEST_ASSERT_EQ(reader.remaining(), 0);
    }
}

} // namespace sse::test
