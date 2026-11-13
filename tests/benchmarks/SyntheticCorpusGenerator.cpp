#include "benchmarks/SyntheticCorpusGenerator.hpp"
#include <fstream>
#include <random>
#include <chrono>
#include <sstream>

namespace sse::benchmark {

static const std::vector<std::string> s_knownTerms = {
    "dijkstra", "algorithm", "python", "guide", "search", "engine",
    "inverted", "index", "ranking", "bm25", "tfidf", "database",
    "binary", "serialization", "persistence", "compaction", "recovery",
    "cache", "memory", "trie", "queue", "stack", "performance"
};

const std::vector<std::string>& SyntheticCorpusGenerator::getKnownTerms() {
    return s_knownTerms;
}

SyntheticCorpusGenerator::SyntheticCorpusGenerator(std::filesystem::path baseDir, uint32_t randomSeed)
    : m_baseDir(std::move(baseDir))
    , m_seed(randomSeed)
{
}

void SyntheticCorpusGenerator::cleanup() {
    std::error_code ec;
    std::filesystem::remove_all(m_baseDir, ec);
}

CorpusGenerationStats SyntheticCorpusGenerator::generateCorpus(uint64_t targetCount, uint32_t maxDepth) {
    cleanup();
    std::filesystem::create_directories(m_baseDir);

    auto startTime = std::chrono::steady_clock::now();
    CorpusGenerationStats stats;
    stats.rootPath = m_baseDir;

    std::mt19937 rng(m_seed);

    static const std::vector<std::string> textExts = {".txt", ".cpp", ".h", ".py", ".md", ".java"};
    static const std::vector<std::string> mediaExts = {".jpg", ".png", ".bmp", ".mp3", ".mp4", ".wav"};
    static const std::vector<std::string> otherExts = {".pdf", ".docx", ".zip", ".bin", ".dat"};

    // Pre-create subdirectories to establish hierarchy
    std::vector<std::filesystem::path> dirPaths;
    dirPaths.push_back(m_baseDir);
    for (uint32_t d = 1; d <= maxDepth; ++d) {
        for (int i = 0; i < 4; ++i) {
            auto sub = dirPaths[(d - 1) * 4] / ("folder_" + std::to_string(d) + "_" + std::to_string(i));
            std::filesystem::create_directories(sub);
            dirPaths.push_back(sub);
        }
    }

    for (uint64_t i = 0; i < targetCount; ++i) {
        // Pick target directory
        const auto& dir = dirPaths[i % dirPaths.size()];

        uint32_t roll = rng() % 100;
        std::string ext;
        std::string filename;

        if (roll < 60) {
            // 60% Text and source code
            stats.textAndCodeFiles++;
            ext = textExts[rng() % textExts.size()];
            filename = "doc_" + std::to_string(i) + ext;
            auto filePath = dir / filename;

            // Generate content
            std::size_t wordCount = 50 + (rng() % 350);
            std::string content = generateDeterministicText(i, wordCount);
            std::ofstream out(filePath, std::ios::binary);
            out.write(content.data(), static_cast<std::streamsize>(content.size()));
            out.close();

            stats.totalBytes += content.size();
        } else if (roll < 85) {
            // 25% Image and media fixture
            stats.mediaAndImageFiles++;
            ext = mediaExts[rng() % mediaExts.size()];
            filename = "media_" + std::to_string(i) + ext;
            auto filePath = dir / filename;

            std::size_t fixtureSize = 256 + (rng() % 2048);
            writeBinaryFixture(filePath, fixtureSize);
            stats.totalBytes += fixtureSize;
        } else {
            // 15% Other formats
            stats.archiveAndOtherFiles++;
            ext = otherExts[rng() % otherExts.size()];
            filename = "archive_" + std::to_string(i) + ext;
            auto filePath = dir / filename;

            std::size_t fixtureSize = 512 + (rng() % 4096);
            writeBinaryFixture(filePath, fixtureSize);
            stats.totalBytes += fixtureSize;
        }

        stats.totalFiles++;
    }

    auto endTime = std::chrono::steady_clock::now();
    stats.generationDurationMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    return stats;
}

std::string SyntheticCorpusGenerator::generateDeterministicText(uint64_t fileIndex, std::size_t targetWordCount) {
    std::mt19937 localRng(static_cast<uint32_t>(fileIndex * 31 + m_seed));
    std::ostringstream ss;

    // Header comment with known terms
    ss << "// Generated synthetic file #" << fileIndex << "\n";
    ss << "// Category: Source or Document. Topic: Data Structures and Algorithms.\n";

    // Embed guaranteed known terms based on modulo patterns for query tests
    if (fileIndex % 2 == 0) ss << "dijkstra algorithm graph shortest path calculation\n";
    if (fileIndex % 3 == 0) ss << "python programming guide tutorial implementation\n";
    if (fileIndex % 5 == 0) ss << "database storage binary serialization persistence\n";
    if (fileIndex % 7 == 0) ss << "inverted index ranking bm25 tfidf scoring\n";

    for (std::size_t w = 0; w < targetWordCount; ++w) {
        if (localRng() % 5 == 0) {
            ss << s_knownTerms[localRng() % s_knownTerms.size()] << " ";
        } else {
            uint32_t tokenNum = localRng() % 5000;
            ss << "term" << tokenNum << " ";
        }
        if (w > 0 && w % 12 == 0) {
            ss << "\n";
        }
    }
    ss << "\n";
    return ss.str();
}

void SyntheticCorpusGenerator::writeBinaryFixture(const std::filesystem::path& path, std::size_t byteCount) {
    std::ofstream out(path, std::ios::binary);
    std::vector<char> buf(byteCount);
    for (std::size_t i = 0; i < byteCount; ++i) {
        buf[i] = static_cast<char>((i * 7 + 13) % 256);
    }
    out.write(buf.data(), static_cast<std::streamsize>(buf.size()));
    out.close();
}

} // namespace sse::benchmark
