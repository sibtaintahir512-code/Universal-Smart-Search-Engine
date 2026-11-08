#include "indexing/ScanWorker.hpp"

namespace sse::indexing {

ScanWorker::ScanWorker() = default;

ScanWorker::~ScanWorker() {
    stopAndJoin();
}

bool ScanWorker::startScan(const std::vector<std::string>& roots,
                           DocumentCatalog& catalog,
                           const ScannerConfig& config,
                           ProgressCallback onProgress,
                           CompletionCallback onComplete,
                           FileCallback onFile) {
    if (m_isRunning.load(std::memory_order_acquire)) {
        return false;
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }

    m_cancelFlag.store(false, std::memory_order_release);
    m_isRunning.store(true, std::memory_order_release);

    m_thread = std::thread([this, roots, &catalog, config, onProgress, onComplete, onFile]() {
        FileScanner scanner(config);
        scanner.setRoots(roots);

        auto progressWrapper = [this, onProgress](const ScanStatistics& stats) {
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                m_latestStats = stats;
            }
            if (onProgress) {
                onProgress(stats);
            }
        };

        ScanStatistics finalStats = scanner.scan(catalog, onFile, progressWrapper, m_cancelFlag);

        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_latestStats = finalStats;
        }

        m_isRunning.store(false, std::memory_order_release);

        if (onComplete) {
            onComplete(finalStats);
        }
    });

    return true;
}

void ScanWorker::cancel() noexcept {
    m_cancelFlag.store(true, std::memory_order_release);
}

void ScanWorker::wait() {
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void ScanWorker::stopAndJoin() {
    cancel();
    wait();
}

ScanStatistics ScanWorker::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_latestStats;
}

} // namespace sse::indexing
