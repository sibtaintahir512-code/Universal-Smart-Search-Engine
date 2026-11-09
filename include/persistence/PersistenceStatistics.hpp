#pragma once

#include <cstdint>
#include <mutex>

namespace sse::persistence {

/**
 * @brief Thread-safe telemetry for index persistence operations.
 */
class PersistenceStatistics {
public:
    PersistenceStatistics() = default;

    void recordSave(double durationMs, uint64_t bytesWritten) noexcept;
    void recordLoad(double durationMs, uint64_t bytesRead) noexcept;
    void recordRecovery() noexcept;
    void recordCompaction(double durationMs) noexcept;
    void reset() noexcept;

    uint32_t totalSaves() const noexcept;
    uint32_t totalLoads() const noexcept;
    uint32_t totalRecoveries() const noexcept;
    uint32_t totalCompactions() const noexcept;
    uint64_t totalBytesWritten() const noexcept;
    uint64_t totalBytesRead() const noexcept;
    double   lastSaveDurationMs() const noexcept;
    double   lastLoadDurationMs() const noexcept;
    double   lastCompactionDurationMs() const noexcept;

private:
    mutable std::mutex m_mutex;
    uint32_t m_saves{0};
    uint32_t m_loads{0};
    uint32_t m_recoveries{0};
    uint32_t m_compactions{0};
    uint64_t m_bytesWritten{0};
    uint64_t m_bytesRead{0};
    double   m_lastSaveMs{0.0};
    double   m_lastLoadMs{0.0};
    double   m_lastCompactionMs{0.0};
};

} // namespace sse::persistence
