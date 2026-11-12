#include "gui/workers/IndexingWorker.hpp"
#include <chrono>

namespace sse::gui {

IndexingWorker::IndexingWorker(SearchEngineAdapter* adapter, QObject* parent)
    : QObject(parent)
    , m_adapter(adapter)
{
}

void IndexingWorker::startIndexing(const QStringList& folders, bool fullRebuild) {
    if (!m_adapter) {
        emit indexingFailed("Search engine adapter unavailable");
        return;
    }

    if (folders.isEmpty()) {
        emit indexingFailed("No root folders specified for indexing");
        return;
    }

    m_cancelToken.store(false, std::memory_order_relaxed);
    emit indexingStarted();

    std::vector<std::string> roots;
    roots.reserve(folders.size());
    for (const auto& f : folders) {
        roots.push_back(f.toStdString());
    }

    auto startTime = std::chrono::steady_clock::now();

    try {
        auto progressCallback = [this](uint64_t discovered, uint64_t indexed, const std::string& currentPath) {
            emit indexingProgress(discovered, indexed, QString::fromStdString(currentPath));
        };

        auto stats = m_adapter->indexFolders(roots, fullRebuild, progressCallback, m_cancelToken);

        auto endTime = std::chrono::steady_clock::now();
        double elapsedSec = std::chrono::duration<double>(endTime - startTime).count();

        if (m_cancelToken.load(std::memory_order_relaxed)) {
            emit indexingCancelled();
        } else {
            emit indexingCompleted(stats.filesDiscovered, stats.filesCataloged, elapsedSec);
        }
    } catch (const std::exception& ex) {
        emit indexingFailed(QString::fromUtf8(ex.what()));
    } catch (...) {
        emit indexingFailed("Unknown error during indexing");
    }
}

void IndexingWorker::cancel() {
    m_cancelToken.store(true, std::memory_order_relaxed);
}

} // namespace sse::gui
