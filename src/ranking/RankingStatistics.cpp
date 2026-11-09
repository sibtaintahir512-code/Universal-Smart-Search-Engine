#include "ranking/RankingStatistics.hpp"
#include <algorithm>

namespace sse::ranking {

RankingStatistics::RankingStatistics(const RankingStatistics& other) {
    std::lock_guard<std::mutex> lock(other.m_mutex);
    m_totalRequests = other.m_totalRequests;
    m_tfidfRequests = other.m_tfidfRequests;
    m_bm25Requests = other.m_bm25Requests;
    m_unrankedRequests = other.m_unrankedRequests;
    m_totalRankedDocuments = other.m_totalRankedDocuments;
    m_totalScoringDurationMs = other.m_totalScoringDurationMs;
    m_maxScoringDurationMs = other.m_maxScoringDurationMs;
    m_totalTopKDurationMs = other.m_totalTopKDurationMs;
    m_rankingFailures = other.m_rankingFailures;
    m_invalidConfigurations = other.m_invalidConfigurations;
}

RankingStatistics& RankingStatistics::operator=(const RankingStatistics& other) {
    if (this != &other) {
        std::scoped_lock lock(m_mutex, other.m_mutex);
        m_totalRequests = other.m_totalRequests;
        m_tfidfRequests = other.m_tfidfRequests;
        m_bm25Requests = other.m_bm25Requests;
        m_unrankedRequests = other.m_unrankedRequests;
        m_totalRankedDocuments = other.m_totalRankedDocuments;
        m_totalScoringDurationMs = other.m_totalScoringDurationMs;
        m_maxScoringDurationMs = other.m_maxScoringDurationMs;
        m_totalTopKDurationMs = other.m_totalTopKDurationMs;
        m_rankingFailures = other.m_rankingFailures;
        m_invalidConfigurations = other.m_invalidConfigurations;
    }
    return *this;
}

void RankingStatistics::recordRanking(RankingMode mode,
                                     std::size_t rankedDocs,
                                     double scoringMs,
                                     double topKMs,
                                     bool failure) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_totalRequests;

    if (failure) {
        ++m_rankingFailures;
        return;
    }

    switch (mode) {
        case RankingMode::TFIDF:
            ++m_tfidfRequests;
            break;
        case RankingMode::BM25:
            ++m_bm25Requests;
            break;
        case RankingMode::DocIdOrder:
            ++m_unrankedRequests;
            break;
    }

    m_totalRankedDocuments += rankedDocs;
    m_totalScoringDurationMs += scoringMs;
    if (scoringMs > m_maxScoringDurationMs) {
        m_maxScoringDurationMs = scoringMs;
    }
    m_totalTopKDurationMs += topKMs;
}

void RankingStatistics::recordInvalidConfiguration() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_invalidConfigurations;
}

void RankingStatistics::reset() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_totalRequests = 0;
    m_tfidfRequests = 0;
    m_bm25Requests = 0;
    m_unrankedRequests = 0;
    m_totalRankedDocuments = 0;
    m_totalScoringDurationMs = 0.0;
    m_maxScoringDurationMs = 0.0;
    m_totalTopKDurationMs = 0.0;
    m_rankingFailures = 0;
    m_invalidConfigurations = 0;
}

uint64_t RankingStatistics::totalRankingRequests() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalRequests;
}

uint64_t RankingStatistics::tfidfRequests() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tfidfRequests;
}

uint64_t RankingStatistics::bm25Requests() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_bm25Requests;
}

uint64_t RankingStatistics::unrankedRequests() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_unrankedRequests;
}

uint64_t RankingStatistics::totalRankedDocuments() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalRankedDocuments;
}

double RankingStatistics::totalScoringDurationMs() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalScoringDurationMs;
}

double RankingStatistics::averageScoringDurationMs() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t scoredRequests = m_tfidfRequests + m_bm25Requests;
    if (scoredRequests == 0) return 0.0;
    return m_totalScoringDurationMs / static_cast<double>(scoredRequests);
}

double RankingStatistics::maxScoringDurationMs() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxScoringDurationMs;
}

double RankingStatistics::totalTopKDurationMs() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalTopKDurationMs;
}

uint64_t RankingStatistics::rankingFailures() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rankingFailures;
}

uint64_t RankingStatistics::invalidConfigurations() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_invalidConfigurations;
}

} // namespace sse::ranking
