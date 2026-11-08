#include "TestHarness.hpp"
#include "extraction/PDFExtractor.hpp"
#include "extraction/OfficeExtractor.hpp"
#include "extraction/ExtractionResult.hpp"
#include "core/CommonTypes.hpp"
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <cstring>
#include <chrono>
#include <atomic>
#include <pugixml.hpp>
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>

namespace sse::test {

using namespace sse::extraction;
using sse::extraction::ExtractionStatus;

namespace {

// Helper to create in-memory ZIP archive
std::vector<uint8_t> createZipArchive(const std::vector<std::pair<std::string, std::string>>& entries) {
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

// Helper to generate a structurally valid minimal PDF 1.4 with Helvetica font
std::string generateValidPdf(const std::vector<std::string>& pagesText) {
    std::size_t N = pagesText.size();
    if (N == 0) N = 1;

    std::size_t fontId = 3 + 2 * N;
    std::string kidsStr;
    for (std::size_t i = 0; i < N; ++i) {
        if (!kidsStr.empty()) kidsStr += " ";
        kidsStr += std::to_string(3 + 2 * i) + " 0 R";
    }

    std::vector<std::string> objects(fontId + 1);
    objects[1] = "<< /Type /Catalog /Pages 2 0 R >>";
    objects[2] = "<< /Type /Pages /Kids [" + kidsStr + "] /Count " + std::to_string(N) + " >>";

    for (std::size_t i = 0; i < N; ++i) {
        std::string txt = i < pagesText.size() ? pagesText[i] : "";
        std::string streamContent = "BT\n/F1 12 Tf\n72 712 Td\n(" + txt + ") Tj\nET";
        std::size_t pageId = 3 + 2 * i;
        std::size_t contentId = 4 + 2 * i;
        objects[pageId] = "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents " +
                          std::to_string(contentId) + " 0 R /Resources << /Font << /F1 " +
                          std::to_string(fontId) + " 0 R >> >> >>";
        objects[contentId] = "<< /Length " + std::to_string(streamContent.size()) + " >>\nstream\n" +
                             streamContent + "\nendstream";
    }

    objects[fontId] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>";

    std::string header = "%PDF-1.4\n";
    std::string body;
    std::vector<std::size_t> offsets(fontId + 1, 0);
    std::size_t curr = header.size();

    for (std::size_t objId = 1; objId <= fontId; ++objId) {
        offsets[objId] = curr;
        std::string objStr = std::to_string(objId) + " 0 obj\n" + objects[objId] + "\nendobj\n";
        body += objStr;
        curr += objStr.size();
    }

    std::size_t xrefOffset = curr;
    std::string xref = "xref\n0 " + std::to_string(fontId + 1) + "\n0000000000 65535 f \n";
    for (std::size_t objId = 1; objId <= fontId; ++objId) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%010zu 00000 n \n", offsets[objId]);
        xref += buf;
    }

    std::string trailer = "trailer\n<< /Size " + std::to_string(fontId + 1) +
                          " /Root 1 0 R >>\nstartxref\n" + std::to_string(xrefOffset) + "\n%%EOF\n";

    return header + body + xref + trailer;
}

} // anonymous namespace

void runPhase42ComplianceTests() {
    std::cout << "\n--- Executing Phase 4.2 PDF Parser Replacement & Extraction Compliance Test Suites ---" << std::endl;

    auto tempDir = std::filesystem::temp_directory_path() / "sse_phase42_compliance_tests";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);

    PDFExtractor pdfExtractor;
    OfficeExtractor officeExtractor;

    // =========================================================================
    // SECTION 1: PDFium Parser Engine Verification
    // =========================================================================
    std::cout << "[RUNNING] Phase 4.2 PDFium Engine Verification Tests..." << std::endl;

    // Test 1.1: Standard Single-Page PDF Extraction
    {
        auto fPdf = tempDir / "test_pdfium_single.pdf";
        std::string pdfContent = generateValidPdf({"Universal Smart Search Engine PDFium Test"});
        { std::ofstream ofs(fPdf, std::ios::binary); ofs << pdfContent; }

        ExtractionResult res = pdfExtractor.extract(fPdf.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.extractedText.find("Universal Smart Search Engine PDFium Test") != std::string::npos);
        TEST_ASSERT(res.metadataSummary.find("EstimatedPages: 1") != std::string::npos);
        TEST_ASSERT(res.extractedByteCount > 0);
        TEST_ASSERT(!res.isTruncated);
    }

    // Test 1.2: Multi-Page PDF Extraction & Reading Order
    {
        auto fPdf = tempDir / "test_pdfium_multipage.pdf";
        std::string pdfContent = generateValidPdf({
            "Page One: Executive Summary and Overview",
            "Page Two: Technical Architecture and Core Data Structures",
            "Page Three: Verification and Quality Assurance Results"
        });
        { std::ofstream ofs(fPdf, std::ios::binary); ofs << pdfContent; }

        ExtractionResult res = pdfExtractor.extract(fPdf.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.metadataSummary.find("EstimatedPages: 3") != std::string::npos);

        std::size_t pos1 = res.extractedText.find("Page One: Executive Summary");
        std::size_t pos2 = res.extractedText.find("Page Two: Technical Architecture");
        std::size_t pos3 = res.extractedText.find("Page Three: Verification");

        TEST_ASSERT(pos1 != std::string::npos);
        TEST_ASSERT(pos2 != std::string::npos);
        TEST_ASSERT(pos3 != std::string::npos);
        // Authoritative sequential page reading order
        TEST_ASSERT(pos1 < pos2);
        TEST_ASSERT(pos2 < pos3);
    }

    // Test 1.3: PDF Output Limit Bounding During Page Traversal
    {
        auto fPdf = tempDir / "test_pdfium_budget.pdf";
        std::string pdfContent = generateValidPdf({
            "Page One: The initial long chapter describing the system foundation in extensive detail.",
            "Page Two: The subsequent chapters covering subsequent stages of the pipeline."
        });
        { std::ofstream ofs(fPdf, std::ios::binary); ofs << pdfContent; }

        // Strictly cap extraction at 25 bytes
        ExtractionResult res = pdfExtractor.extract(fPdf.u8string(), 25);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Partial);
        TEST_ASSERT(res.isTruncated);
        TEST_ASSERT(res.extractedText.size() <= 25);
        TEST_ASSERT(res.extractedByteCount <= 25);
    }

    // Test 1.4: Asynchronous Extraction Cancellation
    {
        auto fPdf = tempDir / "test_pdfium_cancel.pdf";
        std::string pdfContent = generateValidPdf({"Cancelled content test"});
        { std::ofstream ofs(fPdf, std::ios::binary); ofs << pdfContent; }

        std::atomic<bool> cancelToken(true);
        ExtractionResult res = pdfExtractor.extract(fPdf.u8string(), 1024 * 1024, &cancelToken);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Cancelled);
    }

    // Test 1.5: Image-Only / Scanned PDF (No text operators)
    {
        auto fPdf = tempDir / "test_pdfium_scanned.pdf";
        std::string pdfContent = generateValidPdf({""});
        { std::ofstream ofs(fPdf, std::ios::binary); ofs << pdfContent; }

        ExtractionResult res = pdfExtractor.extract(fPdf.u8string(), 1024 * 1024);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::MetadataOnly);
        TEST_ASSERT(res.metadataSummary.find("EstimatedPages: 1") != std::string::npos);
    }

    // Test 1.6: Corrupted PDF Document Handling
    {
        auto fCorrupt = tempDir / "test_pdfium_corrupt.pdf";
        {
            std::ofstream ofs(fCorrupt, std::ios::binary);
            ofs << "NOT_A_VALID_PDF_HEADER_AT_ALL_GARBAGE";
        }

        ExtractionResult res = pdfExtractor.extract(fCorrupt.u8string(), 1024 * 1024);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Corrupted);
    }

    // =========================================================================
    // SECTION 2: XML Parsing Security & Robustness (Task B)
    // =========================================================================
    std::cout << "[RUNNING] Phase 4.2 XML Parsing Security Tests..." << std::endl;

    // Test 2.1: Malformed XML Rejection (Elimination of Unsafe Tag-Stripping Fallback)
    {
        auto fDocx = tempDir / "malformed_xml_no_fallback.docx";
        // Create DOCX with broken XML containing unmatched tags and missing root closure
        std::string brokenXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">\n"
            "<w:body>\n"
            "<w:p><w:r><w:t>Confidential Unclosed Information\n"
            "<w:tagWithoutClosure>"; // Broken syntax

        auto zipBytes = createZipArchive({
            {"[Content_Types].xml", "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"></Types>"},
            {"word/document.xml", brokenXml}
        });

        { std::ofstream ofs(fDocx, std::ios::binary); ofs.write(reinterpret_cast<const char*>(zipBytes.data()), zipBytes.size()); }

        ExtractionResult res = officeExtractor.extract(fDocx.u8string(), 1024 * 1024);
        // With unsafe fallback removed, broken XML MUST NOT extract stripped markup or unclosed text
        TEST_ASSERT(res.extractedText.empty() || res.status == ExtractionStatus::Corrupted);
        TEST_ASSERT(res.status != ExtractionStatus::Success);
    }

    // Test 2.2: XML Entity Expansion & DTD Mitigation (XXE Safety)
    {
        auto fDocx = tempDir / "xxe_injection_mitigation.docx";
        std::string xxeXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE root [\n"
            "  <!ELEMENT root ANY >\n"
            "  <!ENTITY xxe SYSTEM \"file:///non_existent_sensitive_file.txt\" >\n"
            "]>\n"
            "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">\n"
            "<w:body>\n"
            "<w:p><w:r><w:t>&xxe; Safe Text</w:t></w:p>\n"
            "</w:body>\n"
            "</w:document>";

        auto zipBytes = createZipArchive({
            {"[Content_Types].xml", "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"></Types>"},
            {"word/document.xml", xxeXml}
        });

        { std::ofstream ofs(fDocx, std::ios::binary); ofs.write(reinterpret_cast<const char*>(zipBytes.data()), zipBytes.size()); }

        ExtractionResult res = officeExtractor.extract(fDocx.u8string(), 1024 * 1024);
        // XXE entity resolution is disabled in pugixml: external entity must not resolve or leak data
        TEST_ASSERT(res.extractedText.find("non_existent_sensitive_file") == std::string::npos);
    }

    // Test 2.3: Valid XML with Unicode Entities & Character Decoding
    {
        auto fDocx = tempDir / "valid_unicode_xml.docx";
        std::string validXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">\n"
            "<w:body>\n"
            "<w:p><w:r><w:t>Caf&#233; &amp; R&#233;sum&#233; Search</w:t></w:r></w:p>\n"
            "</w:body>\n"
            "</w:document>";

        auto zipBytes = createZipArchive({
            {"[Content_Types].xml", "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"></Types>"},
            {"word/document.xml", validXml}
        });

        { std::ofstream ofs(fDocx, std::ios::binary); ofs.write(reinterpret_cast<const char*>(zipBytes.data()), zipBytes.size()); }

        ExtractionResult res = officeExtractor.extract(fDocx.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.extractedText.find("Café & Résumé Search") != std::string::npos);
    }

    // =========================================================================
    // SECTION 3: ZIP Decompression Security (Task C)
    // =========================================================================
    std::cout << "[RUNNING] Phase 4.2 ZIP Decompression Security Tests..." << std::endl;

    // Test 3.1: Zero Compressed Size with Non-Zero Uncompressed Size Handling
    {
        // Construct raw ZIP local file header with uncompressed size > 0 but compressed size == 0
        std::vector<uint8_t> corruptedZip;
        const char localHeader[] = {
            'P', 'K', 0x03, 0x04, // signature
            20, 0,                // version needed
            0, 0,                 // bit flag
            0, 0,                 // compression method (stored)
            0, 0, 0, 0,           // time/date
            0x12, 0x34, 0x56, 0x78, // crc32
            0x00, 0x00, 0x00, 0x00, // compressed size = 0
            0x00, 0x10, 0x00, 0x00, // uncompressed size = 4096 (> 0!)
            0x04, 0x00,           // file name length = 4
            0x00, 0x00,           // extra field length = 0
            't', 'e', 's', 't'    // file name
        };
        corruptedZip.insert(corruptedZip.end(), localHeader, localHeader + sizeof(localHeader));

        auto fZip = tempDir / "zero_comp_nonzero_uncomp.docx";
        { std::ofstream ofs(fZip, std::ios::binary); ofs.write(reinterpret_cast<const char*>(corruptedZip.data()), corruptedZip.size()); }

        ExtractionResult res = officeExtractor.extract(fZip.u8string(), 1024 * 1024);
        TEST_ASSERT(res.status == ExtractionStatus::Corrupted || res.status == ExtractionStatus::Failed);
    }

    // Test 3.2: Cumulative Decompression Budgeting
    {
        // Create an archive with 10 small text entries
        std::vector<std::pair<std::string, std::string>> entries;
        entries.push_back({"[Content_Types].xml", "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"></Types>"});
        std::string repeated(1000, 'X');
        for (int i = 0; i < 10; ++i) {
            entries.push_back({"entry_" + std::to_string(i) + ".xml", repeated});
        }
        auto zipBytes = createZipArchive(entries);

        auto fZip = tempDir / "cumulative_entries.docx";
        { std::ofstream ofs(fZip, std::ios::binary); ofs.write(reinterpret_cast<const char*>(zipBytes.data()), zipBytes.size()); }

        // Extraction with tight byte limit
        ExtractionResult res = officeExtractor.extract(fZip.u8string(), 250);
        TEST_ASSERT(res.extractedByteCount <= 250 || res.status == ExtractionStatus::Partial || res.status == ExtractionStatus::Corrupted);
    }

    // =========================================================================
    // SECTION 4: XLSX Non-Sequential Worksheet Relationships (Task D)
    // =========================================================================
    std::cout << "[RUNNING] Phase 4.2 XLSX Worksheet Relationship Order Tests..." << std::endl;

    // Test 4.1: Authoritative Workbook Relationship Order vs File Name Sorting
    {
        auto fXlsx = tempDir / "non_sequential_sheets.xlsx";

        // xl/workbook.xml specifies:
        // First sheet is named "Finance Summary" with r:id="rIdA"
        // Second sheet is named "Raw Transactions" with r:id="rIdB"
        std::string workbookXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
            "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
            "  <sheets>\n"
            "    <sheet name=\"Finance Summary\" sheetId=\"1\" r:id=\"rIdA\"/>\n"
            "    <sheet name=\"Raw Transactions\" sheetId=\"2\" r:id=\"rIdB\"/>\n"
            "  </sheets>\n"
            "</workbook>";

        // In xl/_rels/workbook.xml.rels:
        // rIdA -> worksheets/sheetZ.xml (lexicographically SECOND)
        // rIdB -> worksheets/sheetA.xml (lexicographically FIRST)
        std::string workbookRelsXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
            "  <Relationship Id=\"rIdA\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheetZ.xml\"/>\n"
            "  <Relationship Id=\"rIdB\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheetA.xml\"/>\n"
            "</Relationships>";

        // Shared strings containing text for both sheets
        std::string sharedStringsXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<sst xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" count=\"2\" uniqueCount=\"2\">\n"
            "  <si><t>FIRST: Summary Revenue Total 500000</t></si>\n"
            "  <si><t>SECOND: Transaction Detail Records</t></si>\n"
            "</sst>";

        // sheetZ.xml references shared string index 0 ("FIRST: Summary Revenue Total 500000")
        std::string sheetZXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
            "  <sheetData>\n"
            "    <row r=\"1\"><c r=\"A1\" t=\"s\"><v>0</v></c></row>\n"
            "  </sheetData>\n"
            "</worksheet>";

        // sheetA.xml references shared string index 1 ("SECOND: Transaction Detail Records")
        std::string sheetAXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
            "  <sheetData>\n"
            "    <row r=\"1\"><c r=\"A1\" t=\"s\"><v>1</v></c></row>\n"
            "  </sheetData>\n"
            "</worksheet>";

        auto zipBytes = createZipArchive({
            {"[Content_Types].xml", "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"></Types>"},
            {"xl/workbook.xml", workbookXml},
            {"xl/_rels/workbook.xml.rels", workbookRelsXml},
            {"xl/sharedStrings.xml", sharedStringsXml},
            {"xl/worksheets/sheetA.xml", sheetAXml},
            {"xl/worksheets/sheetZ.xml", sheetZXml}
        });

        { std::ofstream ofs(fXlsx, std::ios::binary); ofs.write(reinterpret_cast<const char*>(zipBytes.data()), zipBytes.size()); }

        ExtractionResult res = officeExtractor.extract(fXlsx.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());

        std::size_t posSummary = res.extractedText.find("FIRST: Summary Revenue Total 500000");
        std::size_t posDetail = res.extractedText.find("SECOND: Transaction Detail Records");

        TEST_ASSERT(posSummary != std::string::npos);
        TEST_ASSERT(posDetail != std::string::npos);

        // Crucial validation:
        // Even though sheetA.xml is alphabetically before sheetZ.xml,
        // workbook.xml relationship maps sheetZ first!
        // posSummary MUST appear before posDetail!
        TEST_ASSERT(posSummary < posDetail);
    }

    // Test 4.2: XLSX Inline String Extraction (<is><t>...</t></is>)
    {
        auto fXlsx = tempDir / "inline_strings.xlsx";

        std::string workbookXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
            "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
            "  <sheets>\n"
            "    <sheet name=\"Data\" sheetId=\"1\" r:id=\"rId1\"/>\n"
            "  </sheets>\n"
            "</workbook>";

        std::string workbookRelsXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
            "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
            "</Relationships>";

        std::string sheet1Xml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
            "  <sheetData>\n"
            "    <row r=\"1\">\n"
            "      <c r=\"A1\" t=\"inlineStr\"><is><t>Inline Alpha Header</t></is></c>\n"
            "      <c r=\"B1\" t=\"inlineStr\"><is><t>Inline Beta Value</t></is></c>\n"
            "    </row>\n"
            "  </sheetData>\n"
            "</worksheet>";

        auto zipBytes = createZipArchive({
            {"[Content_Types].xml", "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"></Types>"},
            {"xl/workbook.xml", workbookXml},
            {"xl/_rels/workbook.xml.rels", workbookRelsXml},
            {"xl/worksheets/sheet1.xml", sheet1Xml}
        });

        { std::ofstream ofs(fXlsx, std::ios::binary); ofs.write(reinterpret_cast<const char*>(zipBytes.data()), zipBytes.size()); }

        ExtractionResult res = officeExtractor.extract(fXlsx.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.extractedText.find("Inline Alpha Header") != std::string::npos);
        TEST_ASSERT(res.extractedText.find("Inline Beta Value") != std::string::npos);
    }

    // Test 4.3: Office Extraction Traversal Budgeting
    {
        auto fDocx = tempDir / "traversal_budget.docx";
        std::string longBody =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">\n"
            "<w:body>\n";
        for (int i = 0; i < 50; ++i) {
            longBody += "<w:p><w:r><w:t>Paragraph number " + std::to_string(i) + " with verbose text.</w:t></w:r></w:p>\n";
        }
        longBody += "</w:body></w:document>";

        auto zipBytes = createZipArchive({
            {"[Content_Types].xml", "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"></Types>"},
            {"word/document.xml", longBody}
        });

        { std::ofstream ofs(fDocx, std::ios::binary); ofs.write(reinterpret_cast<const char*>(zipBytes.data()), zipBytes.size()); }

        ExtractionResult res = officeExtractor.extract(fDocx.u8string(), 60);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Partial);
        TEST_ASSERT(res.isTruncated);
        TEST_ASSERT(res.extractedText.size() <= 60);
    }

    std::filesystem::remove_all(tempDir);
}

} // namespace sse::test
