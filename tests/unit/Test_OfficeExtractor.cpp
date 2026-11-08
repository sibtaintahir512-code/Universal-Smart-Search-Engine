#include "TestHarness.hpp"
#include "extraction/OfficeExtractor.hpp"
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstring>
#include <zlib.h>

namespace sse::test {

namespace {
    // Helper to generate a valid standards-compliant minimal ZIP archive in memory
    std::vector<uint8_t> createMinimalZip(const std::string& entryName, const std::string& content) {
        std::vector<uint8_t> zip;
        uint32_t crc = crc32(0L, Z_NULL, 0);
        crc = crc32(crc, reinterpret_cast<const Bytef*>(content.data()), static_cast<uInt>(content.size()));

        uint16_t nameLen = static_cast<uint16_t>(entryName.size());
        uint32_t contentLen = static_cast<uint32_t>(content.size());

        // 1. Local File Header (30 bytes + name + content)
        zip.push_back('P'); zip.push_back('K'); zip.push_back(0x03); zip.push_back(0x04);
        zip.push_back(20); zip.push_back(0); // Version needed
        zip.push_back(0); zip.push_back(0);  // Flags
        zip.push_back(0); zip.push_back(0);  // Compression 0 = stored
        zip.push_back(0); zip.push_back(0); zip.push_back(0); zip.push_back(0); // Time/Date
        // CRC32
        zip.push_back(static_cast<uint8_t>(crc & 0xFF));
        zip.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
        zip.push_back(static_cast<uint8_t>((crc >> 16) & 0xFF));
        zip.push_back(static_cast<uint8_t>((crc >> 24) & 0xFF));
        // Compressed size
        zip.push_back(static_cast<uint8_t>(contentLen & 0xFF));
        zip.push_back(static_cast<uint8_t>((contentLen >> 8) & 0xFF));
        zip.push_back(static_cast<uint8_t>((contentLen >> 16) & 0xFF));
        zip.push_back(static_cast<uint8_t>((contentLen >> 24) & 0xFF));
        // Uncompressed size
        zip.push_back(static_cast<uint8_t>(contentLen & 0xFF));
        zip.push_back(static_cast<uint8_t>((contentLen >> 8) & 0xFF));
        zip.push_back(static_cast<uint8_t>((contentLen >> 16) & 0xFF));
        zip.push_back(static_cast<uint8_t>((contentLen >> 24) & 0xFF));
        // Name length
        zip.push_back(static_cast<uint8_t>(nameLen & 0xFF));
        zip.push_back(static_cast<uint8_t>((nameLen >> 8) & 0xFF));
        // Extra length
        zip.push_back(0); zip.push_back(0);
        // Entry name
        for (char c : entryName) zip.push_back(static_cast<uint8_t>(c));
        // Content
        for (char c : content) zip.push_back(static_cast<uint8_t>(c));

        uint32_t cdOffset = static_cast<uint32_t>(zip.size());

        // 2. Central Directory Header (46 bytes + name)
        zip.push_back('P'); zip.push_back('K'); zip.push_back(0x01); zip.push_back(0x02);
        zip.push_back(20); zip.push_back(0); // Version made by
        zip.push_back(20); zip.push_back(0); // Version needed
        zip.push_back(0); zip.push_back(0);  // Flags
        zip.push_back(0); zip.push_back(0);  // Method 0 = stored
        zip.push_back(0); zip.push_back(0); zip.push_back(0); zip.push_back(0); // Time/Date
        // CRC32
        zip.push_back(static_cast<uint8_t>(crc & 0xFF));
        zip.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
        zip.push_back(static_cast<uint8_t>((crc >> 16) & 0xFF));
        zip.push_back(static_cast<uint8_t>((crc >> 24) & 0xFF));
        // Compressed size
        zip.push_back(static_cast<uint8_t>(contentLen & 0xFF));
        zip.push_back(static_cast<uint8_t>((contentLen >> 8) & 0xFF));
        zip.push_back(static_cast<uint8_t>((contentLen >> 16) & 0xFF));
        zip.push_back(static_cast<uint8_t>((contentLen >> 24) & 0xFF));
        // Uncompressed size
        zip.push_back(static_cast<uint8_t>(contentLen & 0xFF));
        zip.push_back(static_cast<uint8_t>((contentLen >> 8) & 0xFF));
        zip.push_back(static_cast<uint8_t>((contentLen >> 16) & 0xFF));
        zip.push_back(static_cast<uint8_t>((contentLen >> 24) & 0xFF));
        // Name length
        zip.push_back(static_cast<uint8_t>(nameLen & 0xFF));
        zip.push_back(static_cast<uint8_t>((nameLen >> 8) & 0xFF));
        // Extra, comment, disk num, attrs
        zip.push_back(0); zip.push_back(0); // Extra len
        zip.push_back(0); zip.push_back(0); // Comment len
        zip.push_back(0); zip.push_back(0); // Disk start
        zip.push_back(0); zip.push_back(0); // Internal attr
        zip.push_back(0); zip.push_back(0); zip.push_back(0); zip.push_back(0); // External attr
        // Relative offset of LFH
        zip.push_back(0); zip.push_back(0); zip.push_back(0); zip.push_back(0);
        // Entry name
        for (char c : entryName) zip.push_back(static_cast<uint8_t>(c));

        uint32_t cdSize = static_cast<uint32_t>(zip.size() - cdOffset);

        // 3. End of Central Directory (22 bytes)
        zip.push_back('P'); zip.push_back('K'); zip.push_back(0x05); zip.push_back(0x06);
        zip.push_back(0); zip.push_back(0); // Disk 0
        zip.push_back(0); zip.push_back(0); // Disk start 0
        zip.push_back(1); zip.push_back(0); // Entries on this disk
        zip.push_back(1); zip.push_back(0); // Total entries
        // CD size
        zip.push_back(static_cast<uint8_t>(cdSize & 0xFF));
        zip.push_back(static_cast<uint8_t>((cdSize >> 8) & 0xFF));
        zip.push_back(static_cast<uint8_t>((cdSize >> 16) & 0xFF));
        zip.push_back(static_cast<uint8_t>((cdSize >> 24) & 0xFF));
        // CD offset
        zip.push_back(static_cast<uint8_t>(cdOffset & 0xFF));
        zip.push_back(static_cast<uint8_t>((cdOffset >> 8) & 0xFF));
        zip.push_back(static_cast<uint8_t>((cdOffset >> 16) & 0xFF));
        zip.push_back(static_cast<uint8_t>((cdOffset >> 24) & 0xFF));
        // Comment len
        zip.push_back(0); zip.push_back(0);

        return zip;
    }
}

void runOfficeExtractorTests() {
    std::cout << "[RUNNING] OfficeExtractor Tests..." << std::endl;

    using namespace sse::extraction;
    using sse::extraction::ExtractionStatus;

    auto tempDir = std::filesystem::temp_directory_path() / "sse_office_extractor_test";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);

    OfficeExtractor extractor;

    // Test 1: XML Entity Decoding
    {
        std::string raw = "Tom &amp; Jerry &lt;Cartoons&gt; &quot;Classic&quot; &#65;";
        std::string decoded = OfficeExtractor::decodeXmlEntities(raw);
        TEST_ASSERT_EQ(decoded, std::string("Tom & Jerry <Cartoons> \"Classic\" A"));
    }

    // Test 2: Tag Stripping with Paragraph Break
    {
        std::string xml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">\n"
            "  <w:body>\n"
            "    <w:p><w:r><w:t>Paragraph One</w:t></w:r></w:p>\n"
            "    <w:p><w:r><w:t>Paragraph Two &amp; Three</w:t></w:r></w:p>\n"
            "  </w:body>\n"
            "</w:document>";

        std::string text = OfficeExtractor::stripXmlAndDecode(xml);
        TEST_ASSERT(text.find("Paragraph One") != std::string::npos);
        TEST_ASSERT(text.find("Paragraph Two & Three") != std::string::npos);
        TEST_ASSERT(text.find('\n') != std::string::npos); // Paragraph break preserved
    }

    // Test 3: Valid DOCX Extraction from Synthetic Package
    {
        std::string docxXml =
            "<w:document><w:body>"
            "<w:p><w:r><w:t>Smart Search Engine Architecture</w:t></w:r></w:p>"
            "<w:p><w:r><w:t>Section 1: Data Structures</w:t></w:r></w:p>"
            "</w:body></w:document>";

        auto zipBytes = createMinimalZip("word/document.xml", docxXml);
        auto fDocx = tempDir / "test_doc.docx";
        {
            std::ofstream ofs(fDocx, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(zipBytes.data()), zipBytes.size());
        }

        ExtractionResult res = extractor.extract(fDocx.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Success);
        TEST_ASSERT(res.extractedText.find("Smart Search Engine Architecture") != std::string::npos);
        TEST_ASSERT(res.extractedText.find("Section 1: Data Structures") != std::string::npos);
    }

    // Test 4: Valid PPTX Extraction
    {
        std::string pptxXml =
            "<p:sld><p:cSld><p:spTree><p:sp><p:txBody>"
            "<a:p><a:r><a:t>Slide 1: Executive Overview</a:t></a:r></a:p>"
            "</p:txBody></p:sp></p:spTree></p:cSld></p:sld>";

        auto zipBytes = createMinimalZip("ppt/slides/slide1.xml", pptxXml);
        auto fPptx = tempDir / "presentation.pptx";
        {
            std::ofstream ofs(fPptx, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(zipBytes.data()), zipBytes.size());
        }

        ExtractionResult res = extractor.extract(fPptx.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.extractedText.find("Slide 1: Executive Overview") != std::string::npos);
    }

    // Test 5: Valid XLSX Extraction
    {
        std::string xlsxXml =
            "<sst count=\"2\" uniqueCount=\"2\">"
            "<si><t>Q1 Revenue</t></si>"
            "<si><t>$1,500,000</t></si>"
            "</sst>";

        auto zipBytes = createMinimalZip("xl/sharedStrings.xml", xlsxXml);
        auto fXlsx = tempDir / "financials.xlsx";
        {
            std::ofstream ofs(fXlsx, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(zipBytes.data()), zipBytes.size());
        }

        ExtractionResult res = extractor.extract(fXlsx.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.extractedText.find("Q1 Revenue") != std::string::npos);
        TEST_ASSERT(res.extractedText.find("$1,500,000") != std::string::npos);
    }

    // Test 6: Encrypted Document Detection (OLE container magic D0 CF 11 E0)
    {
        auto fEnc = tempDir / "encrypted.docx";
        const uint8_t oleMagic[] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        {
            std::ofstream ofs(fEnc, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(oleMagic), sizeof(oleMagic));
        }

        ExtractionResult res = extractor.extract(fEnc.u8string(), 1024);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Encrypted);
    }

    // Test 7: Corrupted ZIP package
    {
        auto fCorrupt = tempDir / "corrupt.docx";
        { std::ofstream(fCorrupt) << "This is definitely not a ZIP file."; }

        ExtractionResult res = extractor.extract(fCorrupt.u8string(), 1024);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Corrupted);
    }

    std::filesystem::remove_all(tempDir);
}

} // namespace sse::test
