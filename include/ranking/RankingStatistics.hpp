#pragma once

#include <cstdint>
#include <mutex>
#include "ranking/RankingConfig.hpp"

namespace sse::ranking {

/**
 * @brief Thread-safe telemetry statistics for relevance scoring and ranking.
 */
class RankingStatistics {
public:
    RankingStatistics() = default;
    ~RankingStatistics() = default;

    RankingStatistics(const RankingStatistics& other);
    RankingStatistics& operator=(const RankingStatistics& other);

    void recordRanking(RankingMode mode,
                       std::size_t rankedDocs,
                       double scoringMs,
                       double topKMs,
                       bool failure = false) noexcept;

    void recordInvalidConfiguration() noexcept;

    void reset() noexcept;

    // Getters
    uint64_t totalRankingRequests() const noexcept;
    uint64_t tfidfRequests() const noexcept;
    uint64_t bm25Requests() const noexcept;
    uint64_t unrankedRequests() const noexcept;
    uint64_t totalRankedDocuments() const noexcept;
    double   totalScoringDurationMs() const noexcept;
    double   averageScoringDurationMs() const noexcept;
    double   maxScoringDurationMs() const noexcept;
    double   totalTopKDurationMs() const noexcept;
    uint64_t rankingFailures() const noexcept;
    uint64_t invalidConfigurations() const noexcept;

private:
    mutable std::mutex m_mutex;
    uint64_t           m_totalRequests{0};
    uint64_t           m_tfidfRequests{0};
    uint64_t           m_bm25Requests{0};
    uint64_t           m_unrankedRequests{0};
    uint64_t           m_totalRankedDocuments{0};
    double             m_totalScoringDurationMs{0.0};
    double             m_maxScoringDurationMs{0.0};
    double             m_totalTopKDurationMs{0.0};
    uint64_t           m_rankingFailures{0};
    uint64_t           m_invalidConfigurations{0};
};

} // namespace sse::ranking
