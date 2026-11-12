#include "gui/workers/SearchWorker.hpp"

namespace sse::gui {

SearchWorker::SearchWorker(SearchEngineAdapter* adapter, QObject* parent)
    : QObject(parent)
    , m_adapter(adapter)
{
}

void SearchWorker::performSearch(const QString& queryString,
                                 int offset,
                                 int limit,
                                 int rankingModeInt) {
    if (!m_adapter) {
        emit searchFailed("Search engine backend adapter is null");
        return;
    }

    m_cancelToken.store(false, std::memory_order_relaxed);
    emit searchStarted();

    try {
        auto mode = static_cast<ranking::RankingMode>(rankingModeInt);
        auto response = m_adapter->executeSearch(queryString.toStdString(),
                                                 static_cast<std::size_t>(std::max(0, offset)),
                                                 static_cast<std::size_t>(std::max(1, limit)),
                                                 mode,
                                                 &m_cancelToken);

        if (m_cancelToken.load(std::memory_order_relaxed) && response.status == search::SearchStatus::Cancelled) {
            // Cancelled cleanly
            emit searchCompleted(response);
            return;
        }

        emit searchCompleted(response);
    } catch (const std::exception& ex) {
        emit searchFailed(QString::fromUtf8(ex.what()));
    } catch (...) {
        emit searchFailed("Unknown exception occurred during background search");
    }
}

void SearchWorker::cancel() {
    m_cancelToken.store(true, std::memory_order_relaxed);
}

} // namespace sse::gui
