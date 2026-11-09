#include "TestHarness.hpp"
#include "persistence/CRC32.hpp"
#include <string>
#include <vector>

namespace sse::test {

void runCRC32Tests() {
    std::cout << "[RUNNING] CRC32 Tests..." << std::endl;

    using namespace sse::persistence;

    // 1. Standard IEEE 802.3 Known-Answer Test Vector
    // Input: "123456789" (9 bytes)
    // Expected: 0xCBF43926
    {
        const char* input = "123456789";
        uint32_t crc = CRC32::calculate(input, 9);
        TEST_ASSERT_EQ(crc, 0xCBF43926U);
    }

    // 2. Empty Input
    {
        uint32_t crc = CRC32::calculate("", 0);
        TEST_ASSERT_EQ(crc, 0x00000000U);
    }

    // 3. Single Byte Inputs
    {
        const char* b1 = "a";
        uint32_t crc1 = CRC32::calculate(b1, 1);
        TEST_ASSERT_EQ(crc1, 0xE8B7BE43U);

        const char* b2 = "z";
        uint32_t crc2 = CRC32::calculate(b2, 1);
        TEST_ASSERT(crc2 != 0);
        TEST_ASSERT(crc1 != crc2);
    }

    // 4. Incremental / Streaming Evaluation vs Single-Shot
    {
        std::string text = "Universal Smart Search Engine - Phase 8 Persistence Checksum Test";
        uint32_t singleShotCrc = CRC32::calculate(text);

        CRC32 streamingCrc;
        // Feed in varied chunks (1 byte, 5 bytes, remaining)
        streamingCrc.update(text.data(), 1);
        streamingCrc.update(text.data() + 1, 5);
        streamingCrc.update(text.data() + 6, text.size() - 6);

        TEST_ASSERT_EQ(streamingCrc.value(), singleShotCrc);

        // Reset and recompute
        streamingCrc.reset();
        streamingCrc.update(text);
        TEST_ASSERT_EQ(streamingCrc.value(), singleShotCrc);
    }

    // 5. Bit-Flip / Corruption Sensitivity
    {
        std::string original = "Deterministic Index Data Block with multiple records and postings";
        uint32_t origCrc = CRC32::calculate(original);

        // Flip one bit in first char
        std::string corrupted1 = original;
        corrupted1[0] ^= 0x01;
        uint32_t crcCorrupt1 = CRC32::calculate(corrupted1);
        TEST_ASSERT(origCrc != crcCorrupt1);

        // Flip one bit in middle
        std::string corrupted2 = original;
        corrupted2[corrupted2.size() / 2] ^= 0x80;
        uint32_t crcCorrupt2 = CRC32::calculate(corrupted2);
        TEST_ASSERT(origCrc != crcCorrupt2);
        TEST_ASSERT(crcCorrupt1 != crcCorrupt2);
    }

    // 6. Large Buffer Test (64 KB)
    {
        std::string largeData(65536, 'X');
        uint32_t crcLarge = CRC32::calculate(largeData);
        TEST_ASSERT(crcLarge != 0);

        // Incremental in 4KB chunks
        CRC32 chunked;
        for (std::size_t offset = 0; offset < 65536; offset += 4096) {
            chunked.update(largeData.data() + offset, 4096);
        }
        TEST_ASSERT_EQ(chunked.value(), crcLarge);
    }
}

} // namespace sse::test
