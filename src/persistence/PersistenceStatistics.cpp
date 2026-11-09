#include "persistence/PersistenceStatistics.hpp"

namespace sse::persistence {

void PersistenceStatistics::recordSave(double durationMs, uint64_t bytesWritten) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_saves;
    m_bytesWritten += bytesWritten;
    m_lastSaveMs = durationMs;
}

void PersistenceStatistics::recordLoad(double durationMs, uint64_t bytesRead) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_loads;
    m_bytesRead += bytesRead;
    m_lastLoadMs = durationMs;
}

void PersistenceStatistics::recordRecovery() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_recoveries;
}

void PersistenceStatistics::recordCompaction(double durationMs) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_compactions;
    m_lastCompactionMs = durationMs;
}

void PersistenceStatistics::reset() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_saves = 0;
    m_loads = 0;
    m_recoveries = 0;
    m_compactions = 0;
    m_bytesWritten = 0;
    m_bytesRead = 0;
    m_lastSaveMs = 0.0;
    m_lastLoadMs = 0.0;
    m_lastCompactionMs = 0.0;
}

uint32_t PersistenceStatistics::totalSaves() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_saves;
}

uint32_t PersistenceStatistics::totalLoads() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_loads;
}

uint32_t PersistenceStatistics::totalRecoveries() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_recoveries;
}

uint32_t PersistenceStatistics::totalCompactions() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_compactions;
}

uint64_t PersistenceStatistics::totalBytesWritten() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_bytesWritten;
}

uint64_t PersistenceStatistics::totalBytesRead() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_bytesRead;
}

double PersistenceStatistics::lastSaveDurationMs() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastSaveMs;
}

double PersistenceStatistics::lastLoadDurationMs() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastLoadMs;
}

double PersistenceStatistics::lastCompactionDurationMs() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastCompactionMs;
}

} // namespace sse::persistence
