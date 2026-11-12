#pragma once

#include <QObject>
#include <QStringList>
#include <QString>
#include <atomic>
#include "gui/adapters/SearchEngineAdapter.hpp"

namespace sse::gui {

/**
 * @brief Background worker handling folder scanning and document catalog indexing.
 */
class IndexingWorker : public QObject {
    Q_OBJECT

public:
    explicit IndexingWorker(SearchEngineAdapter* adapter, QObject* parent = nullptr);
    ~IndexingWorker() override = default;

public slots:
    void startIndexing(const QStringList& folders, bool fullRebuild);
    void cancel();

signals:
    void indexingStarted();
    void indexingProgress(quint64 discovered, quint64 indexed, const QString& currentPath);
    void indexingCompleted(quint64 discovered, quint64 indexed, double durationSec);
    void indexingCancelled();
    void indexingFailed(const QString& errorMessage);

private:
    SearchEngineAdapter* m_adapter{nullptr};
    std::atomic<bool>    m_cancelToken{false};
};

} // namespace sse::gui
