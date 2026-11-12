#pragma once

#include <QObject>
#include <QString>
#include "gui/adapters/SearchEngineAdapter.hpp"

namespace sse::gui {

/**
 * @brief Background worker managing index serialization, loading, compaction, and recovery.
 */
class PersistenceWorker : public QObject {
    Q_OBJECT

public:
    explicit PersistenceWorker(SearchEngineAdapter* adapter, QObject* parent = nullptr);
    ~PersistenceWorker() override = default;

public slots:
    void saveIndex(const QString& filename = QString());
    void loadIndex(const QString& filename = QString());
    void compactIndex();
    void recoverIndex(const QString& filename = QString());

signals:
    void persistenceStarted(const QString& operationName);
    void persistenceProgress(const QString& stage);
    void persistenceCompleted(bool success, const QString& message, quint64 generation, double durationMs);

private:
    SearchEngineAdapter* m_adapter{nullptr};
};

} // namespace sse::gui
