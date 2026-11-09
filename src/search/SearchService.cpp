#include "search/SearchService.hpp"
#include "search/QueryLexer.hpp"
#include "search/QueryParser.hpp"
#include "search/QueryEvaluator.hpp"
#include <algorithm>

namespace sse::search {

SearchService::SearchService(const indexing::InvertedIndex* index,
                             const indexing::DocumentCatalog* catalog,
                             SearchConfig config) noexcept
    : m_index(index), m_catalog(catalog), m_config(config) {}

std::string SearchService::extractExtension(const std::string& filename) {
    std::size_t dotPos = filename.rfind('.');
    if (dotPos != std::string::npos && dotPos > 0) {
        return filename.substr(dotPos);
    }
    return "";
}

SearchResponse SearchService::search(std::string_view queryString,
                                    std::size_t offset,
                                    std::size_t limit,
                                    const std::atomic<bool>* cancelToken) {
    auto startTime = std::chrono::high_resolution_clock::now();

    // 1. Cooperative early cancellation check
    if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
        m_statistics.recordSearch(false, true, false, 0, 0.0, 0, 0);
        return SearchResponse::makeError(SearchStatus::Cancelled, std::string(queryString),
                                         "Search cancelled prior to execution");
    }

    // 2. Validate empty query
    bool allWhitespace = true;
    for (char c : queryString) {
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            allWhitespace = false;
            break;
        }
    }
    if (queryString.empty() || allWhitespace) {
        auto endTime = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        m_statistics.recordSearch(false, false, true, 0, elapsedMs, 0, 0);
        return SearchResponse::makeError(SearchStatus::EmptyQuery, std::string(queryString),
                                         "Empty query: no search terms provided");
    }

    // 3. Lexical analysis
    QueryLexer lexer(m_config);
    LexResult lexResult = lexer.tokenize(queryString);
    if (!lexResult.success) {
        auto endTime = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        m_statistics.recordSearch(false, false, true, 0, elapsedMs, 0, 0);
        return SearchResponse::makeError(SearchStatus::SyntaxError, std::string(queryString),
                                         lexResult.errorMessage, lexResult.errorOffset);
    }

    // 4. Expression parsing into AST
    QueryParser parser(m_config);
    ParseResult parseResult = parser.parse(lexResult.tokens);
    if (!parseResult.success) {
        auto endTime = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        m_statistics.recordSearch(false, false, true, 0, elapsedMs, 0, 0);
        return SearchResponse::makeError(SearchStatus::SyntaxError, std::string(queryString),
                                         parseResult.errorMessage, parseResult.errorOffset);
    }

    // 5. AST Evaluation
    QueryEvaluator evaluator(m_index, m_catalog);
    EvaluationResult evalResult = evaluator.evaluate(parseResult.root.get(), cancelToken);

    auto endTime = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    if (evalResult.cancelled) {
        m_statistics.recordSearch(false, true, false, 0, elapsedMs,
                                 evalResult.diagnostics.termsEvaluated,
                                 evalResult.diagnostics.booleanOperationsEvaluated);
        return SearchResponse::makeError(SearchStatus::Cancelled, std::string(queryString),
                                         evalResult.errorMessage);
    }

    if (!evalResult.success) {
        m_statistics.recordSearch(false, false, false, 0, elapsedMs,
                                 evalResult.diagnostics.termsEvaluated,
                                 evalResult.diagnostics.booleanOperationsEvaluated);
        return SearchResponse::makeError(SearchStatus::InternalError, std::string(queryString),
                                         evalResult.errorMessage);
    }

    // 6. Pagination & Result Construction
    SearchResponse response;
    response.query = std::string(queryString);
    response.isValid = true;
    response.status = SearchStatus::Success;
    response.totalMatches = evalResult.resultSet.size();
    response.offset = offset;
    response.executionTimeMs = elapsedMs;

    std::size_t effectiveLimit = limit;
    if (effectiveLimit == 0) {
        effectiveLimit = m_config.defaultLimit > 0 ? m_config.defaultLimit : response.totalMatches;
    }
    response.limit = effectiveLimit;

    if (offset < response.totalMatches && m_catalog) {
        std::size_t endIdx = std::min(offset + effectiveLimit, response.totalMatches);
        response.returnedCount = endIdx - offset;

        for (std::size_t i = offset; i < endIdx; ++i) {
            DocID docId = evalResult.resultSet[i];
            response.matchingDocIds.push_back(docId);

            const FileMetadata* meta = m_catalog->getMetadata(docId);
            if (meta) {
                std::string path = m_catalog->getPath(docId);
                std::string filename = m_catalog->getFilename(docId);
                std::string ext = extractExtension(filename);
                bool hasContent = (meta->extractionState == ExtractionStatus::Extracted);

                response.results.push_back(SearchResultItem(
                    docId,
                    std::move(filename),
                    std::move(path),
                    std::move(ext),
                    meta->category,
                    meta->fileSize,
                    meta->lastModifiedTime,
                    hasContent
                ));
            }
        }
    } else {
        response.returnedCount = 0;
    }

    // 7. Telemetry recording
    m_statistics.recordSearch(true, false, false, response.totalMatches, elapsedMs,
                             evalResult.diagnostics.termsEvaluated,
                             evalResult.diagnostics.booleanOperationsEvaluated);

    return response;
}

} // namespace sse::search
