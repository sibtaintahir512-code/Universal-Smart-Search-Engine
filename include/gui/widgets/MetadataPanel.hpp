#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QTextEdit>
#include "search/SearchResult.hpp"

namespace sse::gui {

class SearchEngineAdapter;

/**
 * @brief Metadata inspection panel displaying document identity, filesystem stats,
 * and ranking explanation breakdowns.
 */
class MetadataPanel : public QWidget {
    Q_OBJECT

public:
    explicit MetadataPanel(QWidget* parent = nullptr);
    ~MetadataPanel() override = default;

    void setItem(const search::SearchResultItem* item, const SearchEngineAdapter* adapter);
    void clear();

private:
    QTableWidget* m_table{nullptr};
    QTextEdit*    m_explanationView{nullptr};

    void setupUi();
    void addRow(const QString& property, const QString& value);
};

} // namespace sse::gui
