#include "indexing/ScanStatistics.hpp"

namespace sse::indexing {

void ScanStatistics::start() noexcept {
    reset();
    startTime = std::chrono::steady_clock::now();
}

void ScanStatistics::stop(bool cancelled) noexcept {
    endTime = std::chrono::steady_clock::now();
    isCancelled = cancelled;
    isCompleted = !cancelled;

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    elapsedMilliseconds = static_cast<uint64_t>(duration.count());
}

void ScanStatistics::reset() noexcept {
    directoriesDiscovered = 0;
    directoriesScanned = 0;
    filesDiscovered = 0;
    filesCataloged = 0;
    newFiles = 0;
    modifiedFiles = 0;
    unchangedFiles = 0;
    missingFiles = 0;
    inaccessibleDirectories = 0;
    metadataErrors = 0;
    skippedEntries = 0;
    elapsedMilliseconds = 0;
    isCancelled = false;
    isCompleted = false;
}

double ScanStatistics::filesPerSecond() const noexcept {
    if (elapsedMilliseconds == 0) return 0.0;
    return static_cast<double>(filesDiscovered) / (static_cast<double>(elapsedMilliseconds) / 1000.0);
}

double ScanStatistics::directoriesPerSecond() const noexcept {
    if (elapsedMilliseconds == 0) return 0.0;
    return static_cast<double>(directoriesScanned) / (static_cast<double>(elapsedMilliseconds) / 1000.0);
}

} // namespace sse::indexing
