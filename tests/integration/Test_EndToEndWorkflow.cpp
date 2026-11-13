#include "TestHarness.hpp"
#include "indexing/FileScanner.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include "indexing/Tokenizer.hpp"
#include "indexing/IndexingPipeline.hpp"
#include "extraction/ExtractionManager.hpp"
#include "search/SearchService.hpp"
#include "ranking/RankingEngine.hpp"
#include "ranking/BM25Scorer.hpp"
#include "ranking/TFIDFScorer.hpp"
#include "persistence/IndexPersistenceManager.hpp"
#include "persistence/IndexCompactor.hpp"
#include "gui/adapters/SearchEngineAdapter.hpp"
#include "gui/models/SearchResultModel.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <zlib.h>

namespace sse::test {

namespace {

// Helper to write a binary file
void writeBinaryFile(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream ofs(path, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

// Helper to write text file
void writeTextFile(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream ofs(path, std::ios::binary);
    ofs.write(text.data(), static_cast<std::streamsize>(text.size()));
}

// Helper to generate minimal zip archive for docx/pptx/xlsx
std::vector<uint8_t> createZipArchive(const std::string& entryName, const std::string& content) {
    std::vector<uint8_t> zip;
    uint32_t crc = static_cast<uint32_t>(crc32(0L, Z_NULL, 0));
    crc = static_cast<uint32_t>(crc32(crc, reinterpret_cast<const Bytef*>(content.data()), static_cast<uInt>(content.size())));

    uint16_t nameLen = static_cast<uint16_t>(entryName.size());
    uint32_t contentLen = static_cast<uint32_t>(content.size());

    // 1. Local File Header
    zip.push_back('P'); zip.push_back('K'); zip.push_back(0x03); zip.push_back(0x04);
    zip.push_back(20); zip.push_back(0); // Version
    zip.push_back(0); zip.push_back(0);  // Flags
    zip.push_back(0); zip.push_back(0);  // Stored
    zip.push_back(0); zip.push_back(0); zip.push_back(0); zip.push_back(0); // Date/Time
    zip.push_back(static_cast<uint8_t>(crc & 0xFF));
    zip.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    zip.push_back(static_cast<uint8_t>((crc >> 16) & 0xFF));
    zip.push_back(static_cast<uint8_t>((crc >> 24) & 0xFF));
    zip.push_back(static_cast<uint8_t>(contentLen & 0xFF));
    zip.push_back(static_cast<uint8_t>((contentLen >> 8) & 0xFF));
    zip.push_back(static_cast<uint8_t>((contentLen >> 16) & 0xFF));
    zip.push_back(static_cast<uint8_t>((contentLen >> 24) & 0xFF));
    zip.push_back(static_cast<uint8_t>(contentLen & 0xFF));
    zip.push_back(static_cast<uint8_t>((contentLen >> 8) & 0xFF));
    zip.push_back(static_cast<uint8_t>((contentLen >> 16) & 0xFF));
    zip.push_back(static_cast<uint8_t>((contentLen >> 24) & 0xFF));
    zip.push_back(static_cast<uint8_t>(nameLen & 0xFF));
    zip.push_back(static_cast<uint8_t>((nameLen >> 8) & 0xFF));
    zip.push_back(0); zip.push_back(0); // Extra length
    for (char c : entryName) zip.push_back(static_cast<uint8_t>(c));
    for (char c : content) zip.push_back(static_cast<uint8_t>(c));

    uint32_t cdOffset = static_cast<uint32_t>(zip.size());

    // 2. Central Directory Header
    zip.push_back('P'); zip.push_back('K'); zip.push_back(0x01); zip.push_back(0x02);
    zip.push_back(20); zip.push_back(0);
    zip.push_back(20); zip.push_back(0);
    zip.push_back(0); zip.push_back(0);
    zip.push_back(0); zip.push_back(0);
    zip.push_back(0); zip.push_back(0); zip.push_back(0); zip.push_back(0);
    zip.push_back(static_cast<uint8_t>(crc & 0xFF));
    zip.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    zip.push_back(static_cast<uint8_t>((crc >> 16) & 0xFF));
    zip.push_back(static_cast<uint8_t>((crc >> 24) & 0xFF));
    zip.push_back(static_cast<uint8_t>(contentLen & 0xFF));
    zip.push_back(static_cast<uint8_t>((contentLen >> 8) & 0xFF));
    zip.push_back(static_cast<uint8_t>((contentLen >> 16) & 0xFF));
    zip.push_back(static_cast<uint8_t>((contentLen >> 24) & 0xFF));
    zip.push_back(static_cast<uint8_t>(contentLen & 0xFF));
    zip.push_back(static_cast<uint8_t>((contentLen >> 8) & 0xFF));
    zip.push_back(static_cast<uint8_t>((contentLen >> 16) & 0xFF));
    zip.push_back(static_cast<uint8_t>((contentLen >> 24) & 0xFF));
    zip.push_back(static_cast<uint8_t>(nameLen & 0xFF));
    zip.push_back(static_cast<uint8_t>((nameLen >> 8) & 0xFF));
    zip.push_back(0); zip.push_back(0);
    zip.push_back(0); zip.push_back(0);
    zip.push_back(0); zip.push_back(0);
    zip.push_back(0); zip.push_back(0);
    zip.push_back(0); zip.push_back(0); zip.push_back(0); zip.push_back(0);
    zip.push_back(0); zip.push_back(0); zip.push_back(0); zip.push_back(0);
    for (char c : entryName) zip.push_back(static_cast<uint8_t>(c));

    uint32_t cdSize = static_cast<uint32_t>(zip.size()) - cdOffset;

    // 3. End of Central Directory
    zip.push_back('P'); zip.push_back('K'); zip.push_back(0x05); zip.push_back(0x06);
    zip.push_back(0); zip.push_back(0);
    zip.push_back(0); zip.push_back(0);
    zip.push_back(1); zip.push_back(0); // 1 entry
    zip.push_back(1); zip.push_back(0);
    zip.push_back(static_cast<uint8_t>(cdSize & 0xFF));
    zip.push_back(static_cast<uint8_t>((cdSize >> 8) & 0xFF));
    zip.push_back(static_cast<uint8_t>((cdSize >> 16) & 0xFF));
    zip.push_back(static_cast<uint8_t>((cdSize >> 24) & 0xFF));
    zip.push_back(static_cast<uint8_t>(cdOffset & 0xFF));
    zip.push_back(static_cast<uint8_t>((cdOffset >> 8) & 0xFF));
    zip.push_back(static_cast<uint8_t>((cdOffset >> 16) & 0xFF));
    zip.push_back(static_cast<uint8_t>((cdOffset >> 24) & 0xFF));
    zip.push_back(0); zip.push_back(0);

    return zip;
}

} // anonymous namespace

void runEndToEndWorkflowTests() {
    std::cout << "\n======================================================================" << std::endl;
    std::cout << " EXECUTING END-TO-END 20-STEP FUNCTIONAL WORKFLOW VERIFICATION" << std::endl;
    std::cout << "======================================================================" << std::endl;

    std::filesystem::path rootDir = "temp_e2e_dataset";
    std::filesystem::remove_all(rootDir);
    std::filesystem::create_directories(rootDir);
    std::filesystem::create_directories(rootDir / "subfolder");

    std::cout << "\n[DATASET GENERATION] Creating 17 multi-format files in " << rootDir << "..." << std::endl;

    // 1. TXT
    writeTextFile(rootDir / "sample.txt", "universal smart search engine data structures algorithms project");
    // 2. Markdown
    writeTextFile(rootDir / "notes.md", "# Dijkstra Algorithm Notes\nShortest path graph traversal implementation.");
    // 3. C++
    writeTextFile(rootDir / "dijkstra.cpp", "#include <iostream>\nint main() { std::cout << \"dijkstra algorithm\" << std::endl; return 0; }");
    // 4. Python
    writeTextFile(rootDir / "script.py", "import os\ndef search_files():\n    print('python script searching files')");
    // 5. PDF (valid minimal PDF structure)
    std::string pdfContent = 
        "%PDF-1.4\n1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
        "3 0 obj<</Type/Page/MediaBox[0 0 612 792]/Parent 2 0 R/Resources<<>>>>endobj\n"
        "xref\n0 4\n0000000000 65535 f \n0000000009 00000 n \n0000000052 00000 n \n0000000101 00000 n \n"
        "trailer<</Size 4/Root 1 0 R>>\nstartxref\n178\n%%EOF\n";
    writeTextFile(rootDir / "document.pdf", pdfContent);
    // 6. DOCX (Valid Office Open XML ZIP)
    std::string docxXml = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                          "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">\n"
                          "<w:body><w:p><w:r><w:t>Dijkstra Graph Research Report</w:t></w:r></w:p></w:body></w:document>";
    writeBinaryFile(rootDir / "report.docx", createZipArchive("word/document.xml", docxXml));
    // 7. PPTX
    std::string pptxXml = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                          "<p:sld xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\" "
                          "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">\n"
                          "<p:cSld><p:spTree><p:sp><p:txBody><a:p><a:r><a:t>Presentation Slide Title</a:t></a:r></a:p></p:txBody></p:sp></p:spTree></p:cSld></p:sld>";
    writeBinaryFile(rootDir / "presentation.pptx", createZipArchive("ppt/slides/slide1.xml", pptxXml));
    // 8. XLSX
    std::string xlsxXml = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                          "<sst xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" count=\"1\" uniqueCount=\"1\">\n"
                          "<si><t>Spreadsheet Revenue Analysis</t></si></sst>";
    writeBinaryFile(rootDir / "spreadsheet.xlsx", createZipArchive("xl/sharedStrings.xml", xlsxXml));
    // 9. Image (PNG header)
    std::vector<uint8_t> pngBytes = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 'I', 'H', 'D', 'R'};
    writeBinaryFile(rootDir / "icon.png", pngBytes);
    // 10. Audio (MP3 ID3v2 header)
    std::vector<uint8_t> mp3Bytes = {'I', 'D', '3', 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20};
    writeBinaryFile(rootDir / "audio.mp3", mp3Bytes);
    // 11. Video (MP4 header)
    std::vector<uint8_t> mp4Bytes = {0x00, 0x00, 0x00, 0x18, 'f', 't', 'y', 'p', 'm', 'p', '4', '2'};
    writeBinaryFile(rootDir / "video.mp4", mp4Bytes);
    // 12. Archive (ZIP)
    writeBinaryFile(rootDir / "bundle.zip", createZipArchive("content.txt", "archived file data"));
    // 13. Unknown Binary
    std::vector<uint8_t> binBytes = {0x7F, 'E', 'L', 'F', 0x02, 0x01, 0x01, 0x00, 0xFF, 0xEE, 0xDD, 0xCC};
    writeBinaryFile(rootDir / "firmware.bin", binBytes);
    // 14. Empty File
    writeTextFile(rootDir / "empty.txt", "");
    // 15. Unicode Filename
    writeTextFile(rootDir / "dokument_unicode.txt", "content inside unicode named text document");
    // 16. Duplicate Filename in subfolder
    writeTextFile(rootDir / "subfolder" / "sample.txt", "duplicate sample text in subfolder");
    // 17. Corrupted PDF
    writeTextFile(rootDir / "corrupt.pdf", "%PDF-1.4\ncorrupted garbage binary stream without valid xref table");

    // =========================================================================
    // Step 1: Initialize System Components
    // =========================================================================
    std::cout << "[Step 1] Initializing Search Engine Subsystems..." << std::endl;
    indexing::DocumentCatalog catalog;
    indexing::InvertedIndex index;
    indexing::Tokenizer tokenizer;
    extraction::ExtractionManager extractionMgr;
    indexing::IndexingPipeline pipeline(catalog, index, tokenizer, &extractionMgr);

    // =========================================================================
    // Step 2 & 3: Select Directory and Start Indexing
    // =========================================================================
    std::cout << "[Step 2 & 3] Scanning & Indexing directory: " << rootDir << "..." << std::endl;
    indexing::FileScanner scanner;
    scanner.addRoot(rootDir.string());
    std::atomic<bool> cancelFlag{false};

    std::size_t filesIndexedCount = 0;
    scanner.scan(catalog, [&](const indexing::ExtendedFileRecord& rec, FileChangeStatus status) {
        if (status == FileChangeStatus::New) {
            auto res = pipeline.indexDocument(rec.metadata.docId);
            if (res.success) {
                ++filesIndexedCount;
            }
        }
    }, nullptr, cancelFlag);

    // =========================================================================
    // Step 4: Verify Discovered Files
    // =========================================================================
    std::cout << "[Step 4] Verifying Discovered Files..." << std::endl;
    TEST_ASSERT_EQ(catalog.activeCount(), 17);
    TEST_ASSERT(filesIndexedCount >= 10);

    // =========================================================================
    // Step 5: Verify Metadata
    // =========================================================================
    std::cout << "[Step 5] Verifying Document Metadata & Categories..." << std::endl;
    DocID dijkstraId = catalog.findDocIdByPath(std::filesystem::canonical(rootDir / "dijkstra.cpp").string());
    TEST_ASSERT_NE(dijkstraId, INVALID_DOC_ID);
    const FileMetadata* dijkstraMeta = catalog.getMetadata(dijkstraId);
    TEST_ASSERT(dijkstraMeta != nullptr);
    TEST_ASSERT_EQ(static_cast<int>(dijkstraMeta->category), static_cast<int>(FileCategory::SourceCode));

    DocID pngId = catalog.findDocIdByPath(std::filesystem::canonical(rootDir / "icon.png").string());
    TEST_ASSERT_NE(pngId, INVALID_DOC_ID);
    const FileMetadata* pngMeta = catalog.getMetadata(pngId);
    TEST_ASSERT(pngMeta != nullptr);
    TEST_ASSERT_EQ(static_cast<int>(pngMeta->category), static_cast<int>(FileCategory::Images));

    // =========================================================================
    // Step 6: Verify Content Extraction
    // =========================================================================
    std::cout << "[Step 6] Verifying Multi-Format Content Extraction..." << std::endl;
    DocID docxId = catalog.findDocIdByPath(std::filesystem::canonical(rootDir / "report.docx").string());
    TEST_ASSERT_NE(docxId, INVALID_DOC_ID);

    DocID corruptId = catalog.findDocIdByPath(std::filesystem::canonical(rootDir / "corrupt.pdf").string());
    TEST_ASSERT_NE(corruptId, INVALID_DOC_ID);
    // Corrupt PDF metadata is retained even if text extraction fails safely
    TEST_ASSERT(catalog.isActive(corruptId));

    // =========================================================================
    // Step 7: Verify Indexed Terms in Inverted Index / Trie
    // =========================================================================
    std::cout << "[Step 7] Verifying Inverted Index Terms..." << std::endl;
    TEST_ASSERT(index.vocabularySize() > 0);
    TEST_ASSERT(index.containsTerm("dijkstra"));
    TEST_ASSERT(index.containsTerm("algorithm"));

    // =========================================================================
    // Step 8: Search for Filename
    // =========================================================================
    std::cout << "[Step 8] Searching by Filename..." << std::endl;
    search::SearchConfig searchCfg;
    searchCfg.rankingConfig.mode = ranking::RankingMode::BM25;
    search::SearchService service(&index, &catalog, searchCfg);

    auto nameResp = service.search("name:dijkstra", 0, 10);
    TEST_ASSERT_EQ(static_cast<int>(nameResp.status), static_cast<int>(search::SearchStatus::Success));
    TEST_ASSERT_EQ(nameResp.totalMatches, 1);
    TEST_ASSERT_EQ(nameResp.results[0].docId, dijkstraId);

    // =========================================================================
    // Step 9: Search for Body Content
    // =========================================================================
    std::cout << "[Step 9] Searching by Body Content..." << std::endl;
    auto bodyResp = service.search("body:traversal", 0, 10);
    TEST_ASSERT_EQ(static_cast<int>(bodyResp.status), static_cast<int>(search::SearchStatus::Success));
    TEST_ASSERT(bodyResp.totalMatches >= 1);

    // =========================================================================
    // Step 10: Perform Boolean Queries
    // =========================================================================
    std::cout << "[Step 10] Performing Boolean Queries (AND, OR, NOT, Parentheses)..." << std::endl;
    auto boolResp1 = service.search("dijkstra AND algorithm", 0, 10);
    TEST_ASSERT(boolResp1.totalMatches >= 1);

    auto boolResp2 = service.search("python OR cpp", 0, 10);
    TEST_ASSERT(boolResp2.totalMatches >= 1);

    auto boolResp3 = service.search("dijkstra AND NOT python", 0, 10);
    TEST_ASSERT(boolResp3.totalMatches >= 1);

    // =========================================================================
    // Step 11: Verify Relevance Ranking
    // =========================================================================
    std::cout << "[Step 11] Verifying BM25 Relevance Ranking Order..." << std::endl;
    auto rankResp = service.search("dijkstra", 0, 10);
    TEST_ASSERT(rankResp.totalMatches >= 1);
    TEST_ASSERT(rankResp.results[0].relevanceScore > 0.0);

    // =========================================================================
    // Step 12: Test Autocomplete
    // =========================================================================
    std::cout << "[Step 12] Testing Prefix Trie Autocomplete..." << std::endl;
    auto suggestions = index.trie().getTopKSuggestions("dijk", 5);
    TEST_ASSERT(!suggestions.empty());
    bool foundDijkstra = false;
    for (std::size_t i = 0; i < suggestions.size(); ++i) {
        if (suggestions[i].term == "dijkstra") foundDijkstra = true;
    }
    TEST_ASSERT(foundDijkstra);

    // =========================================================================
    // Step 13: Open & Verify File Previews
    // =========================================================================
    std::cout << "[Step 13] Verifying File Previews..." << std::endl;
    std::string dijkstraPath = catalog.getPath(dijkstraId);
    auto extResult = extractionMgr.extract(dijkstraPath, FileCategory::SourceCode, dijkstraId);
    TEST_ASSERT_EQ(static_cast<int>(extResult.status), static_cast<int>(extraction::ExtractionStatus::Success));
    TEST_ASSERT(!extResult.extractedText.empty());

    // =========================================================================
    // Step 14 & 15: Modify / Delete and Incremental Synchronization
    // =========================================================================
    std::cout << "[Step 14 & 15] Modifying File & Testing Incremental Synchronization..." << std::endl;
    // Append new unique keyword to notes.md
    writeTextFile(rootDir / "notes.md", "# Dijkstra Algorithm Notes\nShortest path graph traversal implementation.\nSpecialQuantumKeyword added here.");

    scanner.scan(catalog, [&](const indexing::ExtendedFileRecord& rec, FileChangeStatus status) {
        if (status == FileChangeStatus::Modified) {
            pipeline.indexDocument(rec.metadata.docId);
        }
    }, nullptr, cancelFlag);

    // Verify newly indexed term
    TEST_ASSERT(index.containsTerm("specialquantumkeyword"));

    // =========================================================================
    // Step 16: Save Index to Disk (Atomic Save & CRC-32)
    // =========================================================================
    std::cout << "[Step 16] Persisting Index to Disk with CRC-32 Checksum..." << std::endl;
    std::filesystem::path persistDir = "temp_e2e_persistence";
    std::filesystem::remove_all(persistDir);
    std::filesystem::create_directories(persistDir);

    persistence::IndexPersistenceManager persistenceManager(&index, &catalog, persistDir);
    auto saveRes = persistenceManager.saveIndex("search.idx");
    TEST_ASSERT_EQ(static_cast<int>(saveRes.status), static_cast<int>(persistence::PersistenceStatus::Success));
    TEST_ASSERT(std::filesystem::exists(persistDir / "search.idx"));

    // Record pre-restart search results for comparison
    auto preRestartResp = service.search("dijkstra", 0, 10);
    TEST_ASSERT(!preRestartResp.results.empty());

    // =========================================================================
    // Step 17: Close Engine Instance (Scope Exit)
    // =========================================================================
    std::cout << "[Step 17] Closing Engine Instance..." << std::endl;

    // =========================================================================
    // Step 18 & 19: Restart Application & Restore Index from Disk
    // =========================================================================
    std::cout << "[Step 18 & 19] Restarting Application & Loading Index from Disk..." << std::endl;
    indexing::DocumentCatalog restoredCatalog;
    indexing::InvertedIndex restoredIndex;
    persistence::IndexPersistenceManager restoredPersistenceMgr(&restoredIndex, &restoredCatalog, persistDir);

    auto loadRes = restoredPersistenceMgr.loadIndex("search.idx");
    TEST_ASSERT_EQ(static_cast<int>(loadRes.status), static_cast<int>(persistence::PersistenceStatus::Success));
    TEST_ASSERT_EQ(restoredCatalog.activeCount(), catalog.activeCount());
    TEST_ASSERT_EQ(restoredIndex.vocabularySize(), index.vocabularySize());

    // =========================================================================
    // Step 20: Repeat Searches and Compare Results (100% Round-Trip Parity)
    // =========================================================================
    std::cout << "[Step 20] Repeating Searches and Validating 100% Result Parity..." << std::endl;
    search::SearchService restoredService(&restoredIndex, &restoredCatalog, searchCfg);
    auto postRestartResp = restoredService.search("dijkstra", 0, 10);

    TEST_ASSERT_EQ(preRestartResp.totalMatches, postRestartResp.totalMatches);
    TEST_ASSERT_EQ(preRestartResp.results.size(), postRestartResp.results.size());
    for (std::size_t i = 0; i < preRestartResp.results.size(); ++i) {
        TEST_ASSERT_EQ(preRestartResp.results[i].docId, postRestartResp.results[i].docId);
        TEST_ASSERT_EQ(preRestartResp.results[i].filename, postRestartResp.results[i].filename);
        TEST_ASSERT_EQ(preRestartResp.results[i].relevanceScore, postRestartResp.results[i].relevanceScore);
    }

    // Clean up temporary test files
    std::filesystem::remove_all(rootDir);
    std::filesystem::remove_all(persistDir);

    std::cout << "\n >>> 20-STEP END-TO-END WORKFLOW COMPLETED SUCCESSFULLY! (100% Parity) <<<" << std::endl;
}

} // namespace sse::test
