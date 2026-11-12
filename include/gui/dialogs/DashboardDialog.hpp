#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include "gui/adapters/SearchEngineAdapter.hpp"

namespace sse::gui {

/**
 * @brief Telemetry dashboard displaying corpus stats, inverted index metrics,
 * and DSA performance health.
 */
class DashboardDialog : public QDialog {
    Q_OBJECT

public:
    explicit DashboardDialog(SearchEngineAdapter* adapter, QWidget* parent = nullptr);
    ~DashboardDialog() override = default;

public slots:
    void refreshMetrics();

private:
    SearchEngineAdapter* m_adapter{nullptr};

    // Metric labels
    QLabel* m_lblCatalogTotal{nullptr};
    QLabel* m_lblCatalogActive{nullptr};
    QLabel* m_lblVocabTerms{nullptr};
    QLabel* m_lblTrieTerms{nullptr};
    QLabel* m_lblTotalPostings{nullptr};
    QLabel* m_lblTrieNodes{nullptr};
    QLabel* m_lblPersistenceGen{nullptr};
    QLabel* m_lblQueriesRun{nullptr};
    QLabel* m_lblAvgLatency{nullptr};

    QPushButton* m_btnRefresh{nullptr};

    void setupUi();
};

} // namespace sse::gui
