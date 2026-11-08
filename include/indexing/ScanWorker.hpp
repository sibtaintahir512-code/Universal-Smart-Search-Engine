#pragma once

#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include "indexing/FileScanner.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/ScanStatistics.hpp"

namespace sse::indexing {

/**
 * @brief Thread controller managing background filesystem scans.
 * 
 * Guarantees:
 * - Decouples scanning from GUI/main thread.
 * - Thread-safe cooperative cancellation.
 * - Safe RAII join on destruction (never detaches or leaks threads).
 * - Safe value-copied progress and completion callbacks.
 */
class ScanWorker {
public:
    using CompletionCallback = std::function<void(const ScanStatistics& stats)>;
    using ProgressCallback = FileScanner::ProgressCallback;
    using FileCallback = FileScanner::FileCallback;

    ScanWorker();
    ~ScanWorker();

    // Non-copyable, non-movable
    ScanWorker(const ScanWorker&) = delete;
    ScanWorker& operator=(const ScanWorker&) = delete;
    ScanWorker(ScanWorker&&) = delete;
    ScanWorker& operator=(ScanWorker&&) = delete;

    /**
     * @brief Launches an asynchronous background scan.
     * @param roots List of directory roots to scan.
     * @param catalog Shared reference to DocumentCatalog.
     * @param config Scanner options.
     * @param onProgress Progress callback invoked from worker thread.
     * @param onComplete Completion callback invoked when worker terminates.
     * @param onFile Optional callback for individual discovered files.
     * @return true if worker started; false if a scan is already running.
     */
    bool startScan(const std::vector<std::string>& roots,
                   DocumentCatalog& catalog,
                   const ScannerConfig& config = ScannerConfig(),
                   ProgressCallback onProgress = nullptr,
                   CompletionCallback onComplete = nullptr,
                   FileCallback onFile = nullptr);

    /**
     * @brief Requests cooperative cancellation of the running scan.
     * Non-blocking.
     */
    void cancel() noexcept;

    /**
     * @brief Waits for the background scan thread to finish.
     * Blocking.
     */
    void wait();

    /**
     * @brief Requests cancellation and blocks until the worker thread has exited safely.
     */
    void stopAndJoin();

    // State queries
    bool isRunning() const noexcept { return m_isRunning.load(std::memory_order_acquire); }
    bool isCancelled() const noexcept { return m_cancelFlag.load(std::memory_order_acquire); }
    ScanStatistics getStatistics() const;

private:
    std::thread             m_thread;
    std::atomic<bool>       m_isRunning{false};
    std::atomic<bool>       m_cancelFlag{false};
    mutable std::mutex      m_statsMutex;
    ScanStatistics          m_latestStats;
};

} // namespace sse::indexing
