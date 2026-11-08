#include "TestHarness.hpp"
#include "extraction/MediaMetadataExtractor.hpp"
#include <fstream>
#include <filesystem>
#include <vector>

namespace sse::test {

void runMediaMetadataExtractorTests() {
    std::cout << "[RUNNING] MediaMetadataExtractor Tests..." << std::endl;

    using namespace sse::extraction;

    auto tempDir = std::filesystem::temp_directory_path() / "sse_media_extractor_test";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);

    MediaMetadataExtractor extractor;

    // Test 1: Category & Extension Support
    {
        TEST_ASSERT(extractor.supportsExtension("mp4"));
        TEST_ASSERT(extractor.supportsExtension("mp3"));
        TEST_ASSERT(extractor.supportsExtension("wav"));
        TEST_ASSERT(extractor.supportsExtension("flac"));
        TEST_ASSERT(extractor.supportsCategory(FileCategory::Videos));
        TEST_ASSERT(extractor.supportsCategory(FileCategory::Audio));
    }

    // Test 2: Valid WAV Header (44.1 kHz, Stereo, 1 second duration)
    {
        auto fWav = tempDir / "tone.wav";
        // 44-byte WAV header: 44100 Hz, 16-bit, 2 channels -> byteRate = 44100 * 2 * 2 = 176400 bytes/sec
        uint32_t byteRate = 176400;
        uint32_t dataBytes = 176400; // Exactly 1.0 second of audio
        uint8_t wavHdr[44] = {
            'R', 'I', 'F', 'F',
            0, 0, 0, 0,
            'W', 'A', 'V', 'E',
            'f', 'm', 't', ' ',
            16, 0, 0, 0, // Subchunk1Size (16)
            1, 0,        // AudioFormat 1 = PCM
            2, 0,        // NumChannels = 2
            0x44, 0xAC, 0x00, 0x00, // SampleRate = 44100 (0xAC44 LE)
            static_cast<uint8_t>(byteRate & 0xFF), static_cast<uint8_t>((byteRate >> 8) & 0xFF),
            static_cast<uint8_t>((byteRate >> 16) & 0xFF), static_cast<uint8_t>((byteRate >> 24) & 0xFF),
            4, 0,        // BlockAlign = 4
            16, 0,       // BitsPerSample = 16
            'd', 'a', 't', 'a',
            static_cast<uint8_t>(dataBytes & 0xFF), static_cast<uint8_t>((dataBytes >> 8) & 0xFF),
            static_cast<uint8_t>((dataBytes >> 16) & 0xFF), static_cast<uint8_t>((dataBytes >> 24) & 0xFF)
        };

        {
            std::ofstream ofs(fWav, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(wavHdr), sizeof(wavHdr));
        }

        ExtractionResult res = extractor.extract(fWav.u8string(), 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.metadataSummary.find("Channels: 2") != std::string::npos);
        TEST_ASSERT(res.metadataSummary.find("SampleRate: 44100") != std::string::npos);
        TEST_ASSERT(res.metadataSummary.find("Duration: 1s") != std::string::npos);
    }

    // Test 3: Valid FLAC Header
    {
        auto fFlac = tempDir / "sample.flac";
        // 'fLaC' + STREAMINFO metadata block (34 bytes)
        // 44100 Hz, 2 channels, total samples = 44100 * 2 = 88200 (2.0s)
        uint8_t flacHdr[42] = {
            'f', 'L', 'a', 'C',
            0, 0, 0, 34, // Metadata block header: STREAMINFO, 34 bytes
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0x0A, 0xC4, // Sample rate high bits
            0x42,       // Sample rate low bits (44100 = 0xAC44) + channels (2-1=1 -> (1<<1)=2 -> 0x02)
            0, 0, 0x01, 0x58, 0x88 // Total samples = 88200 (0x15888)
        };

        {
            std::ofstream ofs(fFlac, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(flacHdr), sizeof(flacHdr));
        }

        ExtractionResult res = extractor.extract(fFlac.u8string(), 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.metadataSummary.find("Channels: 2") != std::string::npos);
    }

    // Test 4: MP4 Atom Extraction
    {
        auto fMp4 = tempDir / "clip.mp4";
        // ftyp + moov -> mvhd atom (timescale = 1000, duration = 5000 -> 5.0 seconds)
        uint8_t mp4Hdr[60] = {
            0, 0, 0, 20, 'f', 't', 'y', 'p', 'i', 's', 'o', 'm', 0, 0, 2, 0, 'm', 'p', '4', '1',
            0, 0, 0, 36, 'm', 'o', 'o', 'v',
            0, 0, 0, 28, 'm', 'v', 'h', 'd',
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Version, flags, creation, mod
            0, 0, 0x03, 0xE8,                   // Timescale = 1000 (0x3E8)
            0, 0, 0x13, 0x88                    // Duration = 5000 (0x1388) -> 5.0s
        };

        {
            std::ofstream ofs(fMp4, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(mp4Hdr), sizeof(mp4Hdr));
        }

        ExtractionResult res = extractor.extract(fMp4.u8string(), 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT(res.metadataSummary.find("Duration: 5s") != std::string::npos);
    }

    std::filesystem::remove_all(tempDir);
}

} // namespace sse::test
