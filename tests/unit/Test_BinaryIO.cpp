#include "TestHarness.hpp"
#include "persistence/BinaryIO.hpp"

namespace sse::test {

void runBinaryIOTests() {
    std::cout << "[RUNNING] BinaryIO Tests..." << std::endl;

    using namespace sse::persistence;

    // 1. Fundamental Types Round-Trip
    {
        BinaryWriter writer;
        writer.writeUint8(0xAB);
        writer.writeUint16(0xCDEF);
        writer.writeUint32(0x12345678);
        writer.writeUint64(0x0123456789ABCDEFULL);
        writer.writeInt32(-42);
        writer.writeDouble(3.141592653589793);
        writer.writeString16("hello");
        writer.writeString32("world");

        BinaryReader reader(writer.data(), writer.size());
        uint8_t u8 = 0;
        uint16_t u16 = 0;
        uint32_t u32 = 0;
        uint64_t u64 = 0;
        int32_t i32 = 0;
        double d = 0.0;
        std::string s16, s32;

        TEST_ASSERT(reader.readUint8(u8));
        TEST_ASSERT_EQ(u8, 0xAB);

        TEST_ASSERT(reader.readUint16(u16));
        TEST_ASSERT_EQ(u16, 0xCDEF);

        TEST_ASSERT(reader.readUint32(u32));
        TEST_ASSERT_EQ(u32, 0x12345678U);

        TEST_ASSERT(reader.readUint64(u64));
        TEST_ASSERT_EQ(u64, 0x0123456789ABCDEFULL);

        TEST_ASSERT(reader.readInt32(i32));
        TEST_ASSERT_EQ(i32, -42);

        TEST_ASSERT(reader.readDouble(d));
        TEST_ASSERT_NEAR(d, 3.141592653589793, 1e-12);

        TEST_ASSERT(reader.readString16(s16));
        TEST_ASSERT_EQ(s16, std::string("hello"));

        TEST_ASSERT(reader.readString32(s32));
        TEST_ASSERT_EQ(s32, std::string("world"));

        TEST_ASSERT_EQ(reader.remaining(), 0);
        TEST_ASSERT(!reader.hasError());
    }

    // 2. Explicit Little-Endian Byte Order Validation
    {
        BinaryWriter writer;
        writer.writeUint32(0x12345678);
        TEST_ASSERT_EQ(writer.size(), 4);
        const uint8_t* b = writer.data();
        TEST_ASSERT_EQ(b[0], 0x78);
        TEST_ASSERT_EQ(b[1], 0x56);
        TEST_ASSERT_EQ(b[2], 0x34);
        TEST_ASSERT_EQ(b[3], 0x12);
    }

    // 3. Bounds-Checking and Unexpected EOF Rejection
    {
        uint8_t shortBuffer[3] = {0x01, 0x02, 0x03};
        BinaryReader reader(shortBuffer, sizeof(shortBuffer));

        uint32_t u32 = 0;
        // Attempting to read 4 bytes from 3-byte buffer must fail safely
        TEST_ASSERT(!reader.readUint32(u32));
        TEST_ASSERT(reader.hasError());
    }

    // 4. String Length Limit Enforcement
    {
        BinaryWriter writer;
        writer.writeUint16(500); // Claims string length is 500
        for (int i = 0; i < 500; ++i) writer.writeUint8('A');

        BinaryReader reader(writer.data(), writer.size());
        std::string out;
        // Allowed maxLength is 100 => must reject string length 500
        TEST_ASSERT(!reader.readString16(out, 100));
        TEST_ASSERT(reader.hasError());
    }

    // 5. Seek and Offset Navigation
    {
        BinaryWriter writer;
        writer.writeUint32(100);
        writer.writeUint32(200);

        BinaryReader reader(writer.data(), writer.size());
        uint32_t val1 = 0, val2 = 0;

        TEST_ASSERT(reader.seek(4));
        TEST_ASSERT(reader.readUint32(val2));
        TEST_ASSERT_EQ(val2, 200U);

        TEST_ASSERT(reader.seek(0));
        TEST_ASSERT(reader.readUint32(val1));
        TEST_ASSERT_EQ(val1, 100U);

        // Seek past end fails
        TEST_ASSERT(!reader.seek(100));
        TEST_ASSERT(reader.hasError());
    }
}

} // namespace sse::test
