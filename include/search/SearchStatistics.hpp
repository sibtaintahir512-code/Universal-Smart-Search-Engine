#pragma once

#include <cstdint>
#include <mutex>

namespace sse::search {

/**
 * @brief Thread-safe telemetry tracking for search query execution.
 */
class SearchStatistics {
public:
    SearchStatistics() = default;
    ~SearchStatistics() = default;

    SearchStatistics(const SearchStatistics& other);
    SearchStatistics& operator=(const SearchStatistics& other);

    void recordSearch(bool success,
                      bool cancelled,
                      bool isInvalid,
                      std::size_t matches,
                      double durationMs,
                      uint64_t terms,
                      uint64_t boolOps) noexcept;

    void reset() noexcept;

    // Getters
    uint64_t totalRequests() const noexcept;
    uint64_t successfulSearches() const noexcept;
    uint64_t invalidQueries() const noexcept;
    uint64_t cancelledSearches() const noexcept;
    uint64_t emptyResultSearches() const noexcept;
    uint64_t totalMatchesReturned() const noexcept;
    double   totalExecutionTimeMs() const noexcept;
    double   averageExecutionTimeMs() const noexcept;
    double   maxExecutionTimeMs() const noexcept;
    uint64_t termsEvaluated() const noexcept;
    uint64_t booleanOperationsEvaluated() const noexcept;

private:
    mutable std::mutex m_mutex;
    uint64_t           m_totalRequests{0};
    uint64_t           m_successfulSearches{0};
    uint64_t           m_invalidQueries{0};
    uint64_t           m_cancelledSearches{0};
    uint64_t           m_emptyResultSearches{0};
    uint64_t           m_totalMatchesReturned{0};
    double             m_totalExecutionTimeMs{0.0};
    double             m_maxExecutionTimeMs{0.0};
    uint64_t           m_termsEvaluated{0};
    uint64_t           m_booleanOperationsEvaluated{0};
};

} // namespace sse::search
