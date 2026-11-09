#pragma once

#include <string>
#include <string_view>
#include <atomic>
#include <chrono>
#include "indexing/InvertedIndex.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "search/SearchConfig.hpp"
#include "search/SearchResponse.hpp"
#include "search/SearchStatistics.hpp"
#include "ranking/RankingEngine.hpp"

namespace sse::search {

/**
 * @brief Central facade service orchestrating lexical analysis, parsing,
 * AST evaluation, relevance ranking, catalog metadata resolution, and result pagination.
 */
class SearchService {
public:
    SearchService(const indexing::InvertedIndex* index,
                  const indexing::DocumentCatalog* catalog,
                  SearchConfig config = SearchConfig{}) noexcept;

    ~SearchService() = default;

    // Rule of Five (Non-copyable due to pointer references, movable)
    SearchService(const SearchService&) = delete;
    SearchService& operator=(const SearchService&) = delete;
    SearchService(SearchService&&) noexcept = default;
    SearchService& operator=(SearchService&&) noexcept = default;

    /**
     * @brief Executes a search query string end-to-end.
     *
     * @param queryString Raw user search query.
     * @param offset 0-based result starting offset for pagination.
     * @param limit Maximum results to return (0 = use config defaultLimit; size_t(-1) = all).
     * @param cancelToken Optional cooperative cancellation flag.
     * @return SearchResponse Populated response DTO.
     */
    SearchResponse search(std::string_view queryString,
                          std::size_t offset = 0,
                          std::size_t limit = 0,
                          const std::atomic<bool>* cancelToken = nullptr);

    /**
     * @brief Access cumulative search engine statistics.
     */
    const SearchStatistics& statistics() const noexcept { return m_statistics; }
    SearchStatistics& statistics() noexcept { return m_statistics; }

    /**
     * @brief Access ranking engine.
     */
    const ranking::RankingEngine& rankingEngine() const noexcept { return m_rankingEngine; }
    ranking::RankingEngine& rankingEngine() noexcept { return m_rankingEngine; }

    /**
     * @brief Resets all telemetry counters.
     */
    void resetStatistics() noexcept {
        m_statistics.reset();
        m_rankingEngine.resetStatistics();
    }

    /**
     * @brief Access current configuration.
     */
    const SearchConfig& config() const noexcept { return m_config; }
    void setConfig(SearchConfig config) noexcept {
        m_config = config;
        m_rankingEngine.setConfig(m_config.rankingConfig);
    }

private:
    const indexing::InvertedIndex*   m_index{nullptr};
    const indexing::DocumentCatalog* m_catalog{nullptr};
    SearchConfig                     m_config;
    SearchStatistics                 m_statistics;
    ranking::RankingEngine           m_rankingEngine;

    static std::string extractExtension(const std::string& filename);
};

} // namespace sse::search
