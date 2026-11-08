#include "TestHarness.hpp"
#include "indexing/FileMetadataManager.hpp"
#include <fstream>
#include <filesystem>

namespace sse::test {

void runFileMetadataManagerTests() {
    std::cout << "[RUNNING] FileMetadataManager Tests..." << std::endl;

    using namespace sse::indexing;

    // Test 1: Category Classification
    {
        TEST_ASSERT_EQ(FileMetadataManager::classifyExtension("txt"), FileCategory::Documents);
        TEST_ASSERT_EQ(FileMetadataManager::classifyExtension("PDF"), FileCategory::Documents);
        TEST_ASSERT_EQ(FileMetadataManager::classifyExtension(".docx"), FileCategory::Documents);
        TEST_ASSERT_EQ(FileMetadataManager::classifyExtension("png"), FileCategory::Images);
        TEST_ASSERT_EQ(FileMetadataManager::classifyExtension("jpg"), FileCategory::Images);
        TEST_ASSERT_EQ(FileMetadataManager::classifyExtension("mp4"), FileCategory::Videos);
        TEST_ASSERT_EQ(FileMetadataManager::classifyExtension("mp3"), FileCategory::Audio);
        TEST_ASSERT_EQ(FileMetadataManager::classifyExtension("cpp"), FileCategory::SourceCode);
        TEST_ASSERT_EQ(FileMetadataManager::classifyExtension("py"), FileCategory::SourceCode);
        TEST_ASSERT_EQ(FileMetadataManager::classifyExtension("zip"), FileCategory::Archives);
        TEST_ASSERT_EQ(FileMetadataManager::classifyExtension("exe"), FileCategory::Executables);
        TEST_ASSERT_EQ(FileMetadataManager::classifyExtension("dll"), FileCategory::Executables);
        TEST_ASSERT_EQ(FileMetadataManager::classifyExtension("xyzUnknown"), FileCategory::Other);
        TEST_ASSERT_EQ(FileMetadataManager::classifyExtension(""), FileCategory::Other);
    }

    // Test 2: Extension Normalization
    {
        TEST_ASSERT_EQ(FileMetadataManager::normalizeExtension(".TXT"), std::string("txt"));
        TEST_ASSERT_EQ(FileMetadataManager::normalizeExtension("CpP"), std::string("cpp"));
        TEST_ASSERT_EQ(FileMetadataManager::normalizeExtension(""), std::string(""));
    }

    // Test 3: MIME Type Detection
    {
        TEST_ASSERT_EQ(FileMetadataManager::detectMimeType("txt"), std::string("text/plain"));
        TEST_ASSERT_EQ(FileMetadataManager::detectMimeType("md"), std::string("text/markdown"));
        TEST_ASSERT_EQ(FileMetadataManager::detectMimeType("pdf"), std::string("application/pdf"));
        TEST_ASSERT_EQ(FileMetadataManager::detectMimeType("png"), std::string("image/png"));
        TEST_ASSERT_EQ(FileMetadataManager::detectMimeType("unknown"), std::string("application/octet-stream"));
    }

    // Test 4: Content Extraction Support Heuristic
    {
        TEST_ASSERT(FileMetadataManager::supportsContentExtraction("txt"));
        TEST_ASSERT(FileMetadataManager::supportsContentExtraction("md"));
        TEST_ASSERT(FileMetadataManager::supportsContentExtraction("cpp"));
        TEST_ASSERT(FileMetadataManager::supportsContentExtraction("pdf"));
        TEST_ASSERT(FileMetadataManager::supportsContentExtraction("docx"));
        TEST_ASSERT(!FileMetadataManager::supportsContentExtraction("png"));
        TEST_ASSERT(!FileMetadataManager::supportsContentExtraction("mp4"));
        TEST_ASSERT(!FileMetadataManager::supportsContentExtraction("exe"));
    }

    // Test 5: Real File Metadata Query
    {
        auto tempDir = std::filesystem::temp_directory_path() / "sse_meta_test";
        std::filesystem::create_directories(tempDir);
        auto testFile = tempDir / "test_doc.txt";

        {
            std::ofstream ofs(testFile);
            ofs << "Hello World Testing 123";
        }

        FileMetadataManager manager;
        ExtendedFileRecord record;
        bool ok = manager.extractMetadata(testFile.u8string(), record);

        TEST_ASSERT(ok);
        TEST_ASSERT_EQ(record.filename, std::string("test_doc.txt"));
        TEST_ASSERT_EQ(record.extension, std::string("txt"));
        TEST_ASSERT_EQ(record.metadata.fileSize, 23ULL);
        TEST_ASSERT_EQ(record.metadata.category, FileCategory::Documents);
        TEST_ASSERT((record.metadata.flags & META_FLAG_IS_ACCESSIBLE) != 0);
        TEST_ASSERT((record.metadata.flags & META_FLAG_COMPLETE) != 0);
        TEST_ASSERT((record.metadata.flags & META_FLAG_SUPPORTS_CONTENT_EXTRACT) != 0);

        // Cleanup
        std::filesystem::remove_all(tempDir);
    }

    // Test 6: Non-Existent File Handling
    {
        FileMetadataManager manager;
        ExtendedFileRecord record;
        bool ok = manager.extractMetadata("non_existent_file_path_12345.xyz", record);
        TEST_ASSERT(!ok);
        TEST_ASSERT((record.metadata.flags & META_FLAG_IS_ACCESSIBLE) == 0);
    }
}

} // namespace sse::test
