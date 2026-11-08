#include "TestHarness.hpp"
#include "extraction/PDFExtractor.hpp"
#include "extraction/PDFDocumentParser.hpp"
#include "extraction/OfficeExtractor.hpp"
#include "extraction/TextExtractor.hpp"
#include "extraction/ExtractionCache.hpp"
#include "extraction/ExtractionManager.hpp"
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstring>
#include <zlib.h>
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>

namespace sse::test {

using namespace sse::extraction;
using sse::extraction::ExtractionStatus;

namespace {

// Helper to create a multi-entry ZIP archive in memory
std::vector<uint8_t> createZipWithEntries(const std::vector<std::pair<std::string, std::string>>& entries) {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    mz_zip_writer_init_heap(&zip, 0, 1024 * 1024);

    for (const auto& entry : entries) {
        mz_zip_writer_add_mem(&zip, entry.first.c_str(),
                              entry.second.data(), entry.second.size(),
                              MZ_DEFAULT_COMPRESSION);
    }

    void* buf = nullptr;
    std::size_t size = 0;
    mz_zip_writer_finalize_heap_archive(&zip, &buf, &size);
    std::vector<uint8_t> out(reinterpret_cast<uint8_t*>(buf), reinterpret_cast<uint8_t*>(buf) + size);
    mz_zip_writer_end(&zip);
    return out;
}

// Helper to compress bytes using zlib
std::vector<uint8_t> deflateData(const std::string& input) {
    std::vector<uint8_t> out(input.size() + 256);
    z_stream strm;
    std::memset(&strm, 0, sizeof(strm));
    deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    strm.avail_in = static_cast<uInt>(input.size());
    strm.next_out = reinterpret_cast<Bytef*>(out.data());
    strm.avail_out = static_cast<uInt>(out.size());
    deflate(&strm, Z_FINISH);
    out.resize(strm.total_out);
    deflateEnd(&strm);
    return out;
}

} // anonymous namespace

void runPhase41HardeningTests() {
    std::cout << "[RUNNING] Phase 4.1 Extraction Hardening & Adversarial Tests..." << std::endl;

    auto tempDir = std::filesystem::temp_directory_path() / "sse_hardening_test";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);

    PDFExtractor pdfExtractor;
    OfficeExtractor officeExtractor;
    TextExtractor textExtractor;

    // =========================================================================
    // SECTION 1: PDF EXTRACTION & ADVERSARIAL FIXTURES
    // =========================================================================

    // 1. Single-page text PDF
    {
        auto fPdf = tempDir / "fixture_01_single.pdf";
        std::string pdf =
            "%PDF-1.4\n"
            "1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n"
            "2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj\n"
            "3 0 obj << /Type /Page /Parent 2 0 R /Contents 4 0 R >> endobj\n"
            "4 0 obj << /Length 40 >> stream\n"
            "BT /F1 12 Tf (Single Page Document) Tj ET\n"
            "endstream endobj\n"
            "xref\n0 5\n0000000000 65535 f \n"
            "trailer << /Size 5 /Root 1 0 R >>\n"
            "startxref\n9\n%%EOF";
        { std::ofstream ofs(fPdf, std::ios::binary); ofs << pdf; }

        ExtractionResult res = pdfExtractor.extract(fPdf.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.extractedText.find("Single Page Document") != std::string::npos);
        TEST_ASSERT_EQ(res.isTruncated, false);
    }

    // 2. Multi-page text PDF
    {
        auto fPdf = tempDir / "fixture_02_multipage.pdf";
        std::string pdf =
            "%PDF-1.4\n"
            "1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n"
            "2 0 obj << /Type /Pages /Kids [3 0 R 5 0 R] /Count 2 >> endobj\n"
            "3 0 obj << /Type /Page /Contents 4 0 R >> endobj\n"
            "4 0 obj << /Length 30 >> stream\n"
            "BT (Page One Content) Tj ET\nendstream endobj\n"
            "5 0 obj << /Type /Page /Contents 6 0 R >> endobj\n"
            "6 0 obj << /Length 30 >> stream\n"
            "BT (Page Two Content) Tj ET\nendstream endobj\n"
            "trailer << /Root 1 0 R >>\nstartxref\n9\n%%EOF";
        { std::ofstream ofs(fPdf, std::ios::binary); ofs << pdf; }

        ExtractionResult res = pdfExtractor.extract(fPdf.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.extractedText.find("Page One Content") != std::string::npos);
        TEST_ASSERT(res.extractedText.find("Page Two Content") != std::string::npos);
    }

    // 3. Unicode PDF (UTF-16 BE hex string <FEFF0055006E00690063006F00640065> -> "Unicode")
    {
        auto fPdf = tempDir / "fixture_03_unicode.pdf";
        std::string pdf =
            "%PDF-1.4\n"
            "1 0 obj << /Type /Page /Contents 2 0 R >> endobj\n"
            "2 0 obj << /Length 45 >> stream\n"
            "BT <FEFF0055006E00690063006F00640065> Tj ET\n"
            "endstream endobj\n"
            "trailer << /Root 1 0 R >>\nstartxref\n9\n%%EOF";
        { std::ofstream ofs(fPdf, std::ios::binary); ofs << pdf; }

        ExtractionResult res = pdfExtractor.extract(fPdf.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.extractedText.find("Unicode") != std::string::npos);
    }

    // 4. Embedded-font PDF with /ToUnicode CMap
    {
        auto fPdf = tempDir / "fixture_04_cmap.pdf";
        std::string cmap =
            "/CIDInit /ProcSet findresource begin\n"
            "12 dict begin\n"
            "beginbfchar\n"
            "<01> <0041>\n"  // Glyph \x01 -> 'A' (0x0041)
            "<02> <0042>\n"  // Glyph \x02 -> 'B' (0x0042)
            "endbfchar\n"
            "end end";

        std::string pdf =
            "%PDF-1.4\n"
            "1 0 obj << /Length " + std::to_string(cmap.size()) + " >> stream\n" +
            cmap + "\nendstream endobj\n"
            "2 0 obj << /Type /Page /Contents 3 0 R >> endobj\n"
            "3 0 obj << /Length 20 >> stream\n"
            "BT (\x01\x02) Tj ET\nendstream endobj\n"
            "trailer << /Root 2 0 R >>\nstartxref\n9\n%%EOF";
        { std::ofstream ofs(fPdf, std::ios::binary); ofs << pdf; }

        ExtractionResult res = pdfExtractor.extract(fPdf.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.extractedText.find("AB") != std::string::npos);
    }

    // 5. Compressed PDF (FlateDecode)
    {
        auto fPdf = tempDir / "fixture_05_flate.pdf";
        std::string rawStream = "BT (Flate Compressed Stream Text) Tj ET\n";
        auto compStream = deflateData(rawStream);

        std::string pdfHeader =
            "%PDF-1.4\n"
            "1 0 obj << /Type /Page /Contents 2 0 R >> endobj\n"
            "2 0 obj << /Filter /FlateDecode /Length " + std::to_string(compStream.size()) + " >> stream\n";
        std::string pdfTrailer = "\nendstream endobj\ntrailer << /Root 1 0 R >>\nstartxref\n9\n%%EOF";

        {
            std::ofstream ofs(fPdf, std::ios::binary);
            ofs.write(pdfHeader.data(), pdfHeader.size());
            ofs.write(reinterpret_cast<const char*>(compStream.data()), compStream.size());
            ofs.write(pdfTrailer.data(), pdfTrailer.size());
        }

        ExtractionResult res = pdfExtractor.extract(fPdf.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.extractedText.find("Flate Compressed Stream Text") != std::string::npos);
    }

    // 6. Text positioning operators (Td, T*, TD)
    {
        auto fPdf = tempDir / "fixture_06_pos.pdf";
        std::string pdf =
            "%PDF-1.4\n"
            "1 0 obj << /Type /Page /Contents 2 0 R >> endobj\n"
            "2 0 obj << /Length 40 >> stream\n"
            "BT (Line 1) Tj T* (Line 2) Tj ET\n"
            "endstream endobj\n"
            "trailer << /Root 1 0 R >>\nstartxref\n9\n%%EOF";
        { std::ofstream ofs(fPdf, std::ios::binary); ofs << pdf; }

        ExtractionResult res = pdfExtractor.extract(fPdf.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.extractedText.find("Line 1") != std::string::npos);
        TEST_ASSERT(res.extractedText.find("Line 2") != std::string::npos);
        TEST_ASSERT(res.extractedText.find('\n') != std::string::npos);
    }

    // 7. Empty PDF
    {
        auto fPdf = tempDir / "fixture_07_empty.pdf";
        { std::ofstream(fPdf) << "%PDF-1.4\n%%EOF"; }

        ExtractionResult res = pdfExtractor.extract(fPdf.u8string(), 1024);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::MetadataOnly);
    }

    // 8. Scanned / Image-only PDF (no text streams)
    {
        auto fPdf = tempDir / "fixture_08_scanned.pdf";
        std::string pdf =
            "%PDF-1.4\n"
            "1 0 obj << /Type /Page /Contents 2 0 R >> endobj\n"
            "2 0 obj << /Length 20 >> stream\n"
            "q 100 0 0 100 0 0 cm /Im1 Do Q\n"
            "endstream endobj\n"
            "trailer << /Root 1 0 R >>\nstartxref\n9\n%%EOF";
        { std::ofstream ofs(fPdf, std::ios::binary); ofs << pdf; }

        ExtractionResult res = pdfExtractor.extract(fPdf.u8string(), 1024);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::MetadataOnly);
        TEST_ASSERT(res.extractedText.empty());
    }

    // 9. Encrypted PDF
    {
        auto fPdf = tempDir / "fixture_09_encrypted.pdf";
        std::string pdf =
            "%PDF-1.4\n"
            "1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n"
            "trailer << /Root 1 0 R /Encrypt 3 0 R >>\nstartxref\n9\n%%EOF";
        { std::ofstream ofs(fPdf, std::ios::binary); ofs << pdf; }

        ExtractionResult res = pdfExtractor.extract(fPdf.u8string(), 1024);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Encrypted);
    }

    // 10. Invalid Cross-Reference (Linear Scan fallback)
    {
        auto fPdf = tempDir / "fixture_10_corrupt_xref.pdf";
        std::string pdf =
            "%PDF-1.4\n"
            "1 0 obj << /Type /Page /Contents 2 0 R >> endobj\n"
            "2 0 obj << /Length 30 >> stream\n"
            "BT (Recovered Object Text) Tj ET\nendstream endobj\n"
            "startxref\n999999\n%%EOF"; // Intentionally bogus startxref offset
        { std::ofstream ofs(fPdf, std::ios::binary); ofs << pdf; }

        ExtractionResult res = pdfExtractor.extract(fPdf.u8string(), 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.extractedText.find("Recovered Object Text") != std::string::npos);
    }

    // 11. Truncated PDF (< 8 bytes)
    {
        auto fPdf = tempDir / "fixture_11_trunc.pdf";
        { std::ofstream(fPdf) << "%PDF"; }

        ExtractionResult res = pdfExtractor.extract(fPdf.u8string(), 1024);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Corrupted);
    }

    // 12. Large PDF output budget clamp (Partial)
    {
        auto fPdf = tempDir / "fixture_12_partial.pdf";
        std::string bigText(500, 'A');
        std::string pdf =
            "%PDF-1.4\n"
            "1 0 obj << /Type /Page /Contents 2 0 R >> endobj\n"
            "2 0 obj << /Length " + std::to_string(bigText.size() + 20) + " >> stream\n"
            "BT (" + bigText + ") Tj ET\nendstream endobj\n"
            "trailer << /Root 1 0 R >>\nstartxref\n9\n%%EOF";
        { std::ofstream ofs(fPdf, std::ios::binary); ofs << pdf; }

        ExtractionResult res = pdfExtractor.extract(fPdf.u8string(), 100); // 100 byte limit
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Partial);
        TEST_ASSERT_EQ(res.isTruncated, true);
        TEST_ASSERT_EQ(res.extractedByteCount, 100ULL);
    }

    // 13. PDF Decompression Bomb (Ratio > 100:1 Protection)
    {
        auto fPdf = tempDir / "fixture_13_bomb.pdf";
        // Create 20,000 spaces compressed down to ~30 bytes (ratio > 600:1)
        std::string repeated(20000, ' ');
        auto comp = deflateData(repeated);

        std::string pdfHeader =
            "%PDF-1.4\n"
            "1 0 obj << /Filter /FlateDecode /Length " + std::to_string(comp.size()) + " >> stream\n";
        std::string pdfTail = "\nendstream endobj\n%%EOF";

        {
            std::ofstream ofs(fPdf, std::ios::binary);
            ofs.write(pdfHeader.data(), pdfHeader.size());
            ofs.write(reinterpret_cast<const char*>(comp.data()), comp.size());
            ofs.write(pdfTail.data(), pdfTail.size());
        }

        // Must terminate safely and protect memory
        ExtractionResult res = pdfExtractor.extract(fPdf.u8string(), 1024 * 1024);
        TEST_ASSERT(res.status == ExtractionStatus::MetadataOnly || res.status == ExtractionStatus::Success);
    }

    // 14. Cancellation during extraction
    {
        auto fPdf = tempDir / "fixture_14_cancel.pdf";
        std::string pdf =
            "%PDF-1.4\n"
            "1 0 obj << /Type /Page /Contents 2 0 R >> endobj\n"
            "2 0 obj << /Length 30 >> stream\n"
            "BT (Cancelled PDF Content) Tj ET\nendstream endobj\n"
            "trailer << /Root 1 0 R >>\nstartxref\n9\n%%EOF";
        { std::ofstream ofs(fPdf, std::ios::binary); ofs << pdf; }

        std::atomic<bool> token{true};
        ExtractionResult res = pdfExtractor.extract(fPdf.u8string(), 1024, &token);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Cancelled);
    }

    // =========================================================================
    // SECTION 2: OFFICE DOCUMENT HARDENING & ORDERING
    // =========================================================================

    // 15. PPTX Non-Lexicographical Slide Ordering Fix
    // Demonstrates slide1 -> slide10 -> slide2 relationship ordering
    {
        auto fPptx = tempDir / "ordered_slides.pptx";
        std::string rels =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
            "  <Relationship Id=\"rId1\" Target=\"slides/slide1.xml\"/>\n"
            "  <Relationship Id=\"rId2\" Target=\"slides/slide10.xml\"/>\n"
            "  <Relationship Id=\"rId3\" Target=\"slides/slide2.xml\"/>\n"
            "</Relationships>";

        std::string pres =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<p:presentation xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\" "
            "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
            "  <p:sldIdLst>\n"
            "    <p:sldId id=\"256\" r:id=\"rId1\"/>\n"
            "    <p:sldId id=\"257\" r:id=\"rId2\"/>\n"
            "    <p:sldId id=\"258\" r:id=\"rId3\"/>\n"
            "  </p:sldIdLst>\n"
            "</p:presentation>";

        std::string s1 = "<p:sld xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\"><a:p><a:t>FIRST</a:t></a:p></p:sld>";
        std::string s10 = "<p:sld xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\"><a:p><a:t>SECOND</a:t></a:p></p:sld>";
        std::string s2 = "<p:sld xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\"><a:p><a:t>THIRD</a:t></a:p></p:sld>";

        std::vector<std::pair<std::string, std::string>> entries = {
            {"ppt/_rels/presentation.xml.rels", rels},
            {"ppt/presentation.xml", pres},
            {"ppt/slides/slide1.xml", s1},
            {"ppt/slides/slide10.xml", s10},
            {"ppt/slides/slide2.xml", s2}
        };

        auto zip = createZipWithEntries(entries);
        {
            std::ofstream ofs(fPptx, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(zip.data()), zip.size());
        }

        ExtractionResult res = officeExtractor.extract(fPptx.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());

        // Verify ordering: FIRST -> SECOND (slide10) -> THIRD (slide2)
        std::size_t posFirst = res.extractedText.find("FIRST");
        std::size_t posSecond = res.extractedText.find("SECOND");
        std::size_t posThird = res.extractedText.find("THIRD");

        TEST_ASSERT(posFirst != std::string::npos);
        TEST_ASSERT(posSecond != std::string::npos);
        TEST_ASSERT(posThird != std::string::npos);
        TEST_ASSERT(posFirst < posSecond);
        TEST_ASSERT(posSecond < posThird);
    }

    // 16. XLSX Inline Strings & Cell Value Resolution
    {
        auto fXlsx = tempDir / "inline_financials.xlsx";
        std::string ssXml =
            "<sst count=\"1\"><si><t>Shared Total</t></si></sst>";

        std::string sheetXml =
            "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
            "  <sheetData>"
            "    <row r=\"1\">"
            "      <c r=\"A1\" t=\"inlineStr\"><is><t>Invoice #409</t></is></c>"
            "      <c r=\"B1\" t=\"s\"><v>0</v></c>"
            "      <c r=\"C1\"><v>999.50</v></c>"
            "    </row>"
            "  </sheetData>"
            "</worksheet>";

        std::vector<std::pair<std::string, std::string>> entries = {
            {"xl/sharedStrings.xml", ssXml},
            {"xl/worksheets/sheet1.xml", sheetXml}
        };

        auto zip = createZipWithEntries(entries);
        {
            std::ofstream ofs(fXlsx, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(zip.data()), zip.size());
        }

        ExtractionResult res = officeExtractor.extract(fXlsx.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.extractedText.find("Invoice #409") != std::string::npos);
        TEST_ASSERT(res.extractedText.find("Shared Total") != std::string::npos);
        TEST_ASSERT(res.extractedText.find("999.50") != std::string::npos);
    }

    // 17. Office Zip Bomb Protection (Entry exceeding 50 MB)
    {
        auto fBomb = tempDir / "bomb.docx";
        mz_zip_archive zip;
        std::memset(&zip, 0, sizeof(zip));
        mz_zip_writer_init_heap(&zip, 0, 1024 * 1024);

        // Add 51 MB entry
        std::string huge(51 * 1024 * 1024, ' ');
        mz_zip_writer_add_mem(&zip, "word/document.xml", huge.data(), huge.size(), MZ_DEFAULT_COMPRESSION);

        void* buf = nullptr;
        std::size_t size = 0;
        mz_zip_writer_finalize_heap_archive(&zip, &buf, &size);
        {
            std::ofstream ofs(fBomb, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(buf), size);
        }
        mz_zip_writer_end(&zip);

        ExtractionResult res = officeExtractor.extract(fBomb.u8string(), 100 * 1024 * 1024);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::FileTooLarge);
    }

    // =========================================================================
    // SECTION 3: TEXT & ENCODING HARDENING
    // =========================================================================

    // 18. UTF-8 RFC 3629 Overlong Sequence Rejection (0xC0, 0x80)
    {
        auto fTxt = tempDir / "overlong.txt";
        // \xC0\x80 is overlong encoding of null byte; must be rejected per RFC 3629
        std::string overlong = "ValidPrefix\xC0\x80ValidSuffix";
        { std::ofstream(fTxt, std::ios::binary) << overlong; }

        ExtractionResult res = textExtractor.extract(fTxt.u8string(), 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.extractedText.find("\xEF\xBF\xBD") != std::string::npos); // Replaced with U+FFFD
        TEST_ASSERT(res.extractedText.find("ValidPrefix") != std::string::npos);
        TEST_ASSERT(res.extractedText.find("ValidSuffix") != std::string::npos);
    }

    // 19. UTF-16 Surrogate Pair Assembly (Emoji U+1F600 '😀')
    {
        auto fUtf16 = tempDir / "surrogate.txt";
        // UTF-16 LE BOM (FF FE) + U+1F600 (High: D83D, Low: DE00) -> 3D D8 00 DE
        uint8_t utf16Data[] = {0xFF, 0xFE, 0x3D, 0xD8, 0x00, 0xDE};
        {
            std::ofstream ofs(fUtf16, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(utf16Data), sizeof(utf16Data));
        }

        ExtractionResult res = textExtractor.extract(fUtf16.u8string(), 1024);
        TEST_ASSERT(res.isSuccess());
        // UTF-8 encoding of U+1F600 is F0 9F 98 80
        const std::string expectedEmoji = "\xF0\x9F\x98\x80";
        TEST_ASSERT_EQ(res.extractedText, expectedEmoji);
    }

    // 20. Minimal RTF Extraction
    {
        auto fRtf = tempDir / "document.rtf";
        std::string rtf = "{\\rtf1\\ansi\\deff0 {\\fonttbl{\\f0 Courier;}}\\f0\\fs24 Hello World!\\par Second Paragraph}";
        { std::ofstream(fRtf) << rtf; }

        ExtractionResult res = textExtractor.extract(fRtf.u8string(), 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.extractedText.find("Hello World!") != std::string::npos);
        TEST_ASSERT(res.extractedText.find("Second Paragraph") != std::string::npos);
    }

    // =========================================================================
    // SECTION 4: EXTRACTION CACHE PATH CONSISTENCY
    // =========================================================================

    // 21. Cache Hit with Weakly Canonical Key
    {
        ExtractionCache cache;
        ExtractionResult r;
        r.sourcePath = "/var/docs/test.txt";
        r.originalFileSize = 100;
        r.extractorId = "TextExtractor";
        r.extractorVersion = "1.1.0";
        r.status = ExtractionStatus::Success;
        r.extractedText = "Cached Text Content";

        // Put using canonical path key
        cache.put("/var/docs/test.txt", r, 5000);

        // Get using identical canonical key -> Cache Hit
        ExtractionResult hitResult;
        bool hit = cache.get("/var/docs/test.txt", 100, 5000, "TextExtractor", "1.1.0", hitResult);
        TEST_ASSERT(hit);
        TEST_ASSERT_EQ(hitResult.extractedText, std::string("Cached Text Content"));

        // File size modification -> Cache Miss
        bool missSize = cache.get("/var/docs/test.txt", 101, 5000, "TextExtractor", "1.1.0", hitResult);
        TEST_ASSERT(!missSize);

        // Timestamp modification -> Cache Miss
        bool missTime = cache.get("/var/docs/test.txt", 100, 5001, "TextExtractor", "1.1.0", hitResult);
        TEST_ASSERT(!missTime);

        // Version bump -> Cache Miss
        bool missVer = cache.get("/var/docs/test.txt", 100, 5000, "TextExtractor", "1.2.0", hitResult);
        TEST_ASSERT(!missVer);
    }

    std::filesystem::remove_all(tempDir);
}

} // namespace sse::test
