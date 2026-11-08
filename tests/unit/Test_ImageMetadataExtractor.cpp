#include "TestHarness.hpp"
#include "extraction/ImageMetadataExtractor.hpp"
#include <fstream>
#include <filesystem>
#include <vector>

namespace sse::test {

void runImageMetadataExtractorTests() {
    std::cout << "[RUNNING] ImageMetadataExtractor Tests..." << std::endl;

    using namespace sse::extraction;
    using sse::extraction::ExtractionStatus;

    auto tempDir = std::filesystem::temp_directory_path() / "sse_image_extractor_test";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);

    ImageMetadataExtractor extractor;

    // Test 1: Valid PNG Header (800 x 600, 8-bit)
    {
        auto fPng = tempDir / "test.png";
        // 8-byte signature + 4-byte chunk len + 4-byte 'IHDR' + 4-byte width (800=0x320) + 4-byte height (600=0x258) + 1-byte bitDepth (8) + ...
        uint8_t pngData[33] = {
            0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, // Signature
            0x00, 0x00, 0x00, 0x0D,                      // IHDR length 13
            'I', 'H', 'D', 'R',                          // IHDR chunk
            0x00, 0x00, 0x03, 0x20,                      // Width: 800
            0x00, 0x00, 0x02, 0x58,                      // Height: 600
            0x08, 0x02, 0x00, 0x00, 0x00,                // 8-bit truecolor
            0x00, 0x00, 0x00, 0x00                       // CRC
        };

        {
            std::ofstream ofs(fPng, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(pngData), sizeof(pngData));
        }

        ExtractionResult res = extractor.extract(fPng.u8string(), 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.metadataSummary.find("Width: 800") != std::string::npos);
        TEST_ASSERT(res.metadataSummary.find("Height: 600") != std::string::npos);
        TEST_ASSERT(res.metadataSummary.find("PNG") != std::string::npos);
    }

    // Test 2: Valid GIF Header (320 x 240)
    {
        auto fGif = tempDir / "test.gif";
        // 'GIF89a' + width (320 = 0x0140 LE: 40 01) + height (240 = 0x00F0 LE: F0 00)
        uint8_t gifData[13] = {
            'G', 'I', 'F', '8', '9', 'a',
            0x40, 0x01, // Width 320
            0xF0, 0x00, // Height 240
            0x80, 0x00, 0x00
        };

        {
            std::ofstream ofs(fGif, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(gifData), sizeof(gifData));
        }

        ExtractionResult res = extractor.extract(fGif.u8string(), 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.metadataSummary.find("Width: 320") != std::string::npos);
        TEST_ASSERT(res.metadataSummary.find("Height: 240") != std::string::npos);
    }

    // Test 3: Valid BMP Header (1920 x 1080)
    {
        auto fBmp = tempDir / "test.bmp";
        // 'BM' + 12-byte header + 4-byte DIB size (40) + width (1920 = 0x0780 LE) + height (1080 = 0x0438 LE)
        uint8_t bmpData[30] = {
            'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0, // Bitmap file header (14B)
            40, 0, 0, 0,                                   // DIB header size (40)
            0x80, 0x07, 0x00, 0x00,                         // Width 1920
            0x38, 0x04, 0x00, 0x00,                         // Height 1080
            1, 0, 24, 0                                     // Planes 1, 24-bit
        };

        {
            std::ofstream ofs(fBmp, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(bmpData), sizeof(bmpData));
        }

        ExtractionResult res = extractor.extract(fBmp.u8string(), 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.metadataSummary.find("Width: 1920") != std::string::npos);
        TEST_ASSERT(res.metadataSummary.find("Height: 1080") != std::string::npos);
    }

    // Test 4: Valid SVG with Title Extraction
    {
        auto fSvg = tempDir / "logo.svg";
        std::string svg =
            "<svg width=\"500\" height=\"300\" xmlns=\"http://www.w3.org/2000/svg\">\n"
            "  <title>Corporate Brand Vector Logo</title>\n"
            "  <circle cx=\"50\" cy=\"50\" r=\"40\" stroke=\"green\" fill=\"yellow\" />\n"
            "</svg>";

        { std::ofstream(fSvg) << svg; }

        ExtractionResult res = extractor.extract(fSvg.u8string(), 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.metadataSummary.find("SVG") != std::string::npos);
        TEST_ASSERT_EQ(res.extractedText, std::string("Corporate Brand Vector Logo"));
    }

    // Test 5: Corrupted Image
    {
        auto fCorrupt = tempDir / "broken.png";
        { std::ofstream(fCorrupt) << "not a real image header"; }

        ExtractionResult res = extractor.extract(fCorrupt.u8string(), 1024);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::MetadataOnly);
    }

    std::filesystem::remove_all(tempDir);
}

} // namespace sse::test
