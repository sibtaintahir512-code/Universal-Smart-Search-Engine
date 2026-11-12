#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include "gui/adapters/SearchEngineAdapter.hpp"
#include "gui/workers/WorkerTypes.hpp"
#include "ranking/RankingConfig.hpp"

namespace sse::gui {

/**
 * @brief Background search worker executing queries off the main GUI thread.
 */
class SearchWorker : public QObject {
    Q_OBJECT

public:
    explicit SearchWorker(SearchEngineAdapter* adapter, QObject* parent = nullptr);
    ~SearchWorker() override = default;

public slots:
    void performSearch(const QString& queryString,
                       int offset = 0,
                       int limit = 50,
                       int rankingModeInt = static_cast<int>(ranking::RankingMode::BM25));
    void cancel();

signals:
    void searchStarted();
    void searchCompleted(const sse::search::SearchResponse& response);
    void searchFailed(const QString& errorMessage);

private:
    SearchEngineAdapter* m_adapter{nullptr};
    std::atomic<bool>    m_cancelToken{false};
};

} // namespace sse::gui
