#include "TestHarness.hpp"
#include "extraction/TextExtractor.hpp"
#include <fstream>
#include <filesystem>

namespace sse::test {

void runTextExtractorTests() {
    std::cout << "[RUNNING] TextExtractor Tests..." << std::endl;

    using namespace sse::extraction;
    using sse::extraction::ExtractionStatus;

    auto tempDir = std::filesystem::temp_directory_path() / "sse_text_extractor_test";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);

    TextExtractor extractor;

    // Test 1: Empty TXT
    {
        auto fEmpty = tempDir / "empty.txt";
        { std::ofstream ofs(fEmpty); }

        ExtractionResult res = extractor.extract(fEmpty.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Success);
        TEST_ASSERT_EQ(res.extractedByteCount, 0ULL);
        TEST_ASSERT_EQ(res.extractedText, std::string(""));
        TEST_ASSERT(!res.isTruncated);
    }

    // Test 2: Standard UTF-8 text with Unicode
    {
        auto fUtf8 = tempDir / "utf8_sample.txt";
        std::string expectedText = "Hello World! Universal Smart Search Engine. اردو دستاویزات";
        {
            std::ofstream ofs(fUtf8, std::ios::binary);
            ofs << expectedText;
        }

        ExtractionResult res = extractor.extract(fUtf8.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT_EQ(res.extractedText, expectedText);
        TEST_ASSERT(!res.isTruncated);
    }

    // Test 3: UTF-8 with BOM (EF BB BF)
    {
        auto fBom = tempDir / "utf8_bom.txt";
        std::string rawContent = "\xEF\xBB\xBFThe quick brown fox jumps over the lazy dog.";
        {
            std::ofstream ofs(fBom, std::ios::binary);
            ofs << rawContent;
        }

        ExtractionResult res = extractor.extract(fBom.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        // BOM must be cleanly stripped from extracted text
        TEST_ASSERT_EQ(res.extractedText, std::string("The quick brown fox jumps over the lazy dog."));
    }

    // Test 4: UTF-16 Little Endian with BOM (FF FE)
    {
        auto fLe = tempDir / "utf16_le.txt";
        // "AB" in UTF-16 LE: BOM (FF FE), 'A' (41 00), 'B' (42 00)
        const uint8_t leBytes[] = {0xFF, 0xFE, 0x41, 0x00, 0x42, 0x00};
        {
            std::ofstream ofs(fLe, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(leBytes), sizeof(leBytes));
        }

        ExtractionResult res = extractor.extract(fLe.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT_EQ(res.extractedText, std::string("AB"));
    }

    // Test 5: UTF-16 Big Endian with BOM (FE FF)
    {
        auto fBe = tempDir / "utf16_be.txt";
        // "AB" in UTF-16 BE: BOM (FE FF), 'A' (00 41), 'B' (00 42)
        const uint8_t beBytes[] = {0xFE, 0xFF, 0x00, 0x41, 0x00, 0x42};
        {
            std::ofstream ofs(fBe, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(beBytes), sizeof(beBytes));
        }

        ExtractionResult res = extractor.extract(fBe.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT_EQ(res.extractedText, std::string("AB"));
    }

    // Test 6: Truncated Extraction with small maxBytes budget
    {
        auto fLarge = tempDir / "budget_test.txt";
        std::string longText(1000, 'X');
        {
            std::ofstream ofs(fLarge);
            ofs << longText;
        }

        ExtractionResult res = extractor.extract(fLarge.u8string(), 100);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Partial);
        TEST_ASSERT(res.isTruncated);
        TEST_ASSERT_EQ(res.extractedByteCount, 100ULL);
        TEST_ASSERT_EQ(res.originalFileSize, 1000ULL);
    }

    // Test 7: Cancellation Token
    {
        auto fCancel = tempDir / "cancel.txt";
        { std::ofstream(fCancel) << "Some cancelable content"; }

        std::atomic<bool> cancelFlag{true}; // Already requested
        ExtractionResult res = extractor.extract(fCancel.u8string(), 1024, &cancelFlag);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Cancelled);
    }

    // Test 8: Missing File
    {
        ExtractionResult res = extractor.extract("non_existent_file_xyz_999.txt", 1024);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Failed);
    }

    std::filesystem::remove_all(tempDir);
}

} // namespace sse::test
