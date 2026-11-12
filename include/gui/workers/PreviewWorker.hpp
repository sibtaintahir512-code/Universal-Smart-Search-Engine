#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include "gui/adapters/SearchEngineAdapter.hpp"
#include "gui/workers/WorkerTypes.hpp"

namespace sse::gui {

/**
 * @brief Background worker generating file preview data and reading content off-thread.
 */
class PreviewWorker : public QObject {
    Q_OBJECT

public:
    explicit PreviewWorker(SearchEngineAdapter* adapter, QObject* parent = nullptr);
    ~PreviewWorker() override = default;

public slots:
    void loadPreview(quint32 docId,
                     const QString& filePath,
                     int categoryInt,
                     const QString& searchQuery = QString());
    void cancel();

signals:
    void previewReady(const sse::gui::PreviewData& data);
    void previewFailed(quint32 docId, const QString& errorMessage);

private:
    SearchEngineAdapter* m_adapter{nullptr};
    std::atomic<bool>    m_cancelToken{false};
};

} // namespace sse::gui
