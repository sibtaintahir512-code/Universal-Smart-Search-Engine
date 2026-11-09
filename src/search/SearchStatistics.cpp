#include "search/SearchStatistics.hpp"
#include <algorithm>

namespace sse::search {

SearchStatistics::SearchStatistics(const SearchStatistics& other) {
    std::lock_guard<std::mutex> lock(other.m_mutex);
    m_totalRequests = other.m_totalRequests;
    m_successfulSearches = other.m_successfulSearches;
    m_invalidQueries = other.m_invalidQueries;
    m_cancelledSearches = other.m_cancelledSearches;
    m_emptyResultSearches = other.m_emptyResultSearches;
    m_totalMatchesReturned = other.m_totalMatchesReturned;
    m_totalExecutionTimeMs = other.m_totalExecutionTimeMs;
    m_maxExecutionTimeMs = other.m_maxExecutionTimeMs;
    m_termsEvaluated = other.m_termsEvaluated;
    m_booleanOperationsEvaluated = other.m_booleanOperationsEvaluated;
}

SearchStatistics& SearchStatistics::operator=(const SearchStatistics& other) {
    if (this != &other) {
        std::scoped_lock lock(m_mutex, other.m_mutex);
        m_totalRequests = other.m_totalRequests;
        m_successfulSearches = other.m_successfulSearches;
        m_invalidQueries = other.m_invalidQueries;
        m_cancelledSearches = other.m_cancelledSearches;
        m_emptyResultSearches = other.m_emptyResultSearches;
        m_totalMatchesReturned = other.m_totalMatchesReturned;
        m_totalExecutionTimeMs = other.m_totalExecutionTimeMs;
        m_maxExecutionTimeMs = other.m_maxExecutionTimeMs;
        m_termsEvaluated = other.m_termsEvaluated;
        m_booleanOperationsEvaluated = other.m_booleanOperationsEvaluated;
    }
    return *this;
}

void SearchStatistics::recordSearch(bool success,
                                  bool cancelled,
                                  bool isInvalid,
                                  std::size_t matches,
                                  double durationMs,
                                  uint64_t terms,
                                  uint64_t boolOps) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_totalRequests;

    if (cancelled) {
        ++m_cancelledSearches;
    } else if (isInvalid) {
        ++m_invalidQueries;
    } else if (success) {
        ++m_successfulSearches;
        if (matches == 0) {
            ++m_emptyResultSearches;
        }
        m_totalMatchesReturned += matches;
    }

    m_totalExecutionTimeMs += durationMs;
    if (durationMs > m_maxExecutionTimeMs) {
        m_maxExecutionTimeMs = durationMs;
    }

    m_termsEvaluated += terms;
    m_booleanOperationsEvaluated += boolOps;
}

void SearchStatistics::reset() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_totalRequests = 0;
    m_successfulSearches = 0;
    m_invalidQueries = 0;
    m_cancelledSearches = 0;
    m_emptyResultSearches = 0;
    m_totalMatchesReturned = 0;
    m_totalExecutionTimeMs = 0.0;
    m_maxExecutionTimeMs = 0.0;
    m_termsEvaluated = 0;
    m_booleanOperationsEvaluated = 0;
}

uint64_t SearchStatistics::totalRequests() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalRequests;
}

uint64_t SearchStatistics::successfulSearches() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_successfulSearches;
}

uint64_t SearchStatistics::invalidQueries() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_invalidQueries;
}

uint64_t SearchStatistics::cancelledSearches() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cancelledSearches;
}

uint64_t SearchStatistics::emptyResultSearches() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_emptyResultSearches;
}

uint64_t SearchStatistics::totalMatchesReturned() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalMatchesReturned;
}

double SearchStatistics::totalExecutionTimeMs() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalExecutionTimeMs;
}

double SearchStatistics::averageExecutionTimeMs() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_totalRequests == 0) return 0.0;
    return m_totalExecutionTimeMs / static_cast<double>(m_totalRequests);
}

double SearchStatistics::maxExecutionTimeMs() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxExecutionTimeMs;
}

uint64_t SearchStatistics::termsEvaluated() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_termsEvaluated;
}

uint64_t SearchStatistics::booleanOperationsEvaluated() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_booleanOperationsEvaluated;
}

} // namespace sse::search
