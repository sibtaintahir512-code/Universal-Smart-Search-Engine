#include "gui/workers/PersistenceWorker.hpp"

namespace sse::gui {

PersistenceWorker::PersistenceWorker(SearchEngineAdapter* adapter, QObject* parent)
    : QObject(parent)
    , m_adapter(adapter)
{
}

void PersistenceWorker::saveIndex(const QString& filename) {
    if (!m_adapter) {
        emit persistenceCompleted(false, "Adapter unavailable", 0, 0.0);
        return;
    }

    emit persistenceStarted("Saving index to disk...");
    auto res = m_adapter->saveIndex(filename.toStdString());
    emit persistenceCompleted(res.isSuccess(), QString::fromStdString(res.message), res.generation, res.durationMs);
}

void PersistenceWorker::loadIndex(const QString& filename) {
    if (!m_adapter) {
        emit persistenceCompleted(false, "Adapter unavailable", 0, 0.0);
        return;
    }

    emit persistenceStarted("Loading index from disk...");
    auto res = m_adapter->loadIndex(filename.toStdString());
    emit persistenceCompleted(res.isSuccess(), QString::fromStdString(res.message), res.generation, res.durationMs);
}

void PersistenceWorker::compactIndex() {
    if (!m_adapter) {
        emit persistenceCompleted(false, "Adapter unavailable", 0, 0.0);
        return;
    }

    emit persistenceStarted("Compacting index and remapping DocIDs...");
    auto res = m_adapter->compactIndex();
    emit persistenceCompleted(res.isSuccess(), QString::fromStdString(res.message), res.generation, res.durationMs);
}

void PersistenceWorker::recoverIndex(const QString& filename) {
    if (!m_adapter) {
        emit persistenceCompleted(false, "Adapter unavailable", 0, 0.0);
        return;
    }

    emit persistenceStarted("Recovering index from crash backups...");
    auto res = m_adapter->recoverIndex(filename.toStdString());
    emit persistenceCompleted(res.isSuccess(), QString::fromStdString(res.message), res.generation, res.durationMs);
}

} // namespace sse::gui
