#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>
#include <map>
#include "core/CommonTypes.hpp"

namespace sse::benchmark {

struct CorpusGenerationStats {
    std::filesystem::path rootPath;
    uint64_t totalFiles{0};
    uint64_t totalBytes{0};
    uint32_t textAndCodeFiles{0};
    uint32_t mediaAndImageFiles{0};
    uint32_t archiveAndOtherFiles{0};
    uint32_t uniqueVocabularyCount{0};
    double   generationDurationMs{0.0};
};

/**
 * @brief Deterministic synthetic corpus generator for benchmarks and integration testing.
 */
class SyntheticCorpusGenerator {
public:
    explicit SyntheticCorpusGenerator(std::filesystem::path baseDir = "temp_synthetic_corpus",
                                      uint32_t randomSeed = 42);
    ~SyntheticCorpusGenerator() = default;

    /**
     * @brief Generates a deterministic corpus of N files.
     * @param targetCount Target file count (e.g. 1,000, 10,000, 100,000).
     * @param maxDepth Maximum subfolder hierarchy nesting depth.
     */
    CorpusGenerationStats generateCorpus(uint64_t targetCount, uint32_t maxDepth = 4);

    /**
     * @brief Cleans up and deletes generated files.
     */
    void cleanup();

    const std::filesystem::path& getRootPath() const noexcept { return m_baseDir; }

    // Static known terms embedded into generated files for query verification
    static const std::vector<std::string>& getKnownTerms();

private:
    std::filesystem::path m_baseDir;
    uint32_t              m_seed;

    std::string generateDeterministicText(uint64_t fileIndex, std::size_t targetWordCount);
    void writeBinaryFixture(const std::filesystem::path& path, std::size_t byteCount);
};

} // namespace sse::benchmark
