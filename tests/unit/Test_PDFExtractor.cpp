#include "TestHarness.hpp"
#include "extraction/PDFExtractor.hpp"
#include <fstream>
#include <filesystem>

namespace sse::test {

void runPDFExtractorTests() {
    std::cout << "[RUNNING] PDFExtractor Tests..." << std::endl;

    using namespace sse::extraction;
    using sse::extraction::ExtractionStatus;

    auto tempDir = std::filesystem::temp_directory_path() / "sse_pdf_extractor_test";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);

    PDFExtractor extractor;

    // Test 1: Valid PDF with uncompressed text stream
    {
        auto fPdf = tempDir / "valid.pdf";
        std::string pdfContent =
            "%PDF-1.4\n"
            "1 0 obj\n"
            "<< /Type /Catalog /Pages 2 0 R >>\n"
            "endobj\n"
            "2 0 obj\n"
            "<< /Type /Pages /Kids [3 0 R] /Count 1 >>\n"
            "endobj\n"
            "3 0 obj\n"
            "<< /Type /Page /Parent 2 0 R /Contents 4 0 R >>\n"
            "endobj\n"
            "4 0 obj\n"
            "<< /Length 45 >>\n"
            "stream\n"
            "BT\n"
            "/F1 12 Tf\n"
            "(Universal Search Engine PDF Extraction) Tj\n"
            "ET\n"
            "endstream\n"
            "endobj\n"
            "xref\n"
            "trailer << /Size 5 /Root 1 0 R >>\n"
            "startxref\n"
            "100\n"
            "%%EOF\n";

        { std::ofstream(fPdf, std::ios::binary) << pdfContent; }

        ExtractionResult res = extractor.extract(fPdf.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.extractedText.find("Universal Search Engine PDF Extraction") != std::string::npos);
        TEST_ASSERT(res.metadataSummary.find("EstimatedPages: 1") != std::string::npos);
    }

    // Test 2: Encrypted PDF Detection
    {
        auto fEnc = tempDir / "protected.pdf";
        std::string encPdf =
            "%PDF-1.4\n"
            "1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n"
            "trailer << /Size 2 /Root 1 0 R /Encrypt 3 0 R >>\n"
            "%%EOF\n";

        { std::ofstream(fEnc, std::ios::binary) << encPdf; }

        ExtractionResult res = extractor.extract(fEnc.u8string(), 1024);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Encrypted);
    }

    // Test 3: Corrupted PDF Detection (Missing %PDF- header)
    {
        auto fCorrupt = tempDir / "corrupted.pdf";
        { std::ofstream(fCorrupt) << "Garbage data without PDF header magic"; }

        ExtractionResult res = extractor.extract(fCorrupt.u8string(), 1024);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Corrupted);
    }

    // Test 4: Image-Only Scanned PDF Fallback (MetadataOnly)
    {
        auto fScan = tempDir / "scanned_image.pdf";
        std::string scannedPdf =
            "%PDF-1.4\n"
            "1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n"
            "2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj\n"
            "3 0 obj << /Type /Page /Parent 2 0 R >> endobj\n" // No text streams
            "%%EOF\n";

        { std::ofstream(fScan, std::ios::binary) << scannedPdf; }

        ExtractionResult res = extractor.extract(fScan.u8string(), 1024);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::MetadataOnly);
    }

    std::filesystem::remove_all(tempDir);
}

} // namespace sse::test
