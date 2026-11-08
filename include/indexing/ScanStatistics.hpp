#pragma once

#include <cstdint>
#include <chrono>

namespace sse::indexing {

/**
 * @brief Thread-safe telemetry snapshot of file scanner execution.
 */
struct ScanStatistics {
    uint64_t directoriesDiscovered{0};
    uint64_t directoriesScanned{0};
    uint64_t filesDiscovered{0};
    uint64_t filesCataloged{0};
    uint64_t newFiles{0};
    uint64_t modifiedFiles{0};
    uint64_t unchangedFiles{0};
    uint64_t missingFiles{0};
    uint64_t inaccessibleDirectories{0};
    uint64_t metadataErrors{0};
    uint64_t skippedEntries{0};

    uint64_t elapsedMilliseconds{0};
    bool     isCancelled{false};
    bool     isCompleted{false};

    std::chrono::steady_clock::time_point startTime{};
    std::chrono::steady_clock::time_point endTime{};

    void start() noexcept;
    void stop(bool cancelled = false) noexcept;
    void reset() noexcept;

    double filesPerSecond() const noexcept;
    double directoriesPerSecond() const noexcept;
};

} // namespace sse::indexing
