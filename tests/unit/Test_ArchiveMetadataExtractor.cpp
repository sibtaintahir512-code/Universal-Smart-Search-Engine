#include "TestHarness.hpp"
#include "extraction/ArchiveMetadataExtractor.hpp"
#include <fstream>
#include <filesystem>
#include <vector>

namespace sse::test {

void runArchiveMetadataExtractorTests() {
    std::cout << "[RUNNING] ArchiveMetadataExtractor Tests..." << std::endl;

    using namespace sse::extraction;
    using sse::extraction::ExtractionStatus;

    auto tempDir = std::filesystem::temp_directory_path() / "sse_archive_extractor_test";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);

    ArchiveMetadataExtractor extractor;

    // Test 1: Category & Extension Support
    {
        TEST_ASSERT(extractor.supportsExtension("zip"));
        TEST_ASSERT(extractor.supportsExtension("7z"));
        TEST_ASSERT(extractor.supportsExtension("rar"));
        TEST_ASSERT(extractor.supportsExtension("tar"));
        TEST_ASSERT(extractor.supportsExtension("gz"));
        TEST_ASSERT(extractor.supportsCategory(FileCategory::Archives));
    }

    // Test 2: Valid ZIP Archive Metadata (Single Entry, EOCD parsed)
    {
        auto fZip = tempDir / "test.zip";
        // Minimal ZIP with empty EOCD: PK\x05\x06 + 18 bytes (total 22 bytes), 1 entry
        uint8_t zipData[22] = {
            'P', 'K', 0x05, 0x06,
            0, 0, // Disk 0
            0, 0, // Disk start 0
            1, 0, // Entries this disk (1)
            1, 0, // Total entries (1)
            0, 0, 0, 0, // CD size
            0, 0, 0, 0, // CD offset
            0, 0        // Comment len
        };

        {
            std::ofstream ofs(fZip, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(zipData), sizeof(zipData));
        }

        ExtractionResult res = extractor.extract(fZip.u8string(), 1024);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::MetadataOnly);
        TEST_ASSERT(res.metadataSummary.find("TotalEntries: 1") != std::string::npos);
        // Strict Invariant: No text extracted from archive payload
        TEST_ASSERT(!res.hasText());
    }

    // Test 3: GZIP Header
    {
        auto fGz = tempDir / "archive.gz";
        uint8_t gzData[10] = {0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0, 3};
        {
            std::ofstream ofs(fGz, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(gzData), sizeof(gzData));
        }

        ExtractionResult res = extractor.extract(fGz.u8string(), 1024);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::MetadataOnly);
        TEST_ASSERT(res.metadataSummary.find("GZIP") != std::string::npos);
    }

    // Test 4: 7-Zip Signature
    {
        auto f7z = tempDir / "archive.7z";
        uint8_t szData[6] = {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C};
        {
            std::ofstream ofs(f7z, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(szData), sizeof(szData));
        }

        ExtractionResult res = extractor.extract(f7z.u8string(), 1024);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::MetadataOnly);
        TEST_ASSERT(res.metadataSummary.find("7-Zip") != std::string::npos);
    }

    // Test 5: RAR Signature
    {
        auto fRar = tempDir / "archive.rar";
        uint8_t rarData[7] = {'R', 'a', 'r', '!', 0x1A, 0x07, 0x00};
        {
            std::ofstream ofs(fRar, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(rarData), sizeof(rarData));
        }

        ExtractionResult res = extractor.extract(fRar.u8string(), 1024);
        TEST_ASSERT_EQ(res.status, ExtractionStatus::MetadataOnly);
        TEST_ASSERT(res.metadataSummary.find("RAR") != std::string::npos);
    }

    std::filesystem::remove_all(tempDir);
}

} // namespace sse::test
