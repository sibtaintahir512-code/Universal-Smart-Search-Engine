#pragma once

#include <QAbstractTableModel>
#include <QDateTime>
#include <vector>
#include <cstdint>
#include <string>
#include "search/SearchResult.hpp"
#include "search/SearchResponse.hpp"
#include "core/CommonTypes.hpp"

namespace sse::gui {

/**
 * @brief Filter settings applied to search results view.
 */
struct ResultFilterCriteria {
    bool includeDocuments{true};
    bool includeCode{true};
    bool includeImages{true};
    bool includeMedia{true};
    bool includeArchives{true};
    bool includeOther{true};

    uint64_t minSizeBytes{0};
    uint64_t maxSizeBytes{UINT64_MAX};

    uint64_t minModifiedTimestamp{0}; // Epoch seconds (0 = any)

    bool matches(const search::SearchResultItem& item) const;
};

/**
 * @brief Table model for virtualized rendering of ranked search results.
 */
class SearchResultModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Columns {
        ColCategory = 0,
        ColFilename,
        ColScore,
        ColSize,
        ColModified,
        ColPath,
        ColumnCount
    };

    enum CustomRoles {
        DocIdRole = Qt::UserRole + 1,
        RawScoreRole,
        RawSizeRole,
        RawTimestampRole,
        CategoryRole,
        CanonicalPathRole
    };

    explicit SearchResultModel(QObject* parent = nullptr);
    ~SearchResultModel() override = default;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    void setSearchResponse(const search::SearchResponse& response);
    void clear();

    void setFilterCriteria(const ResultFilterCriteria& criteria);
    const ResultFilterCriteria& filterCriteria() const noexcept { return m_criteria; }

    const search::SearchResultItem* getItem(int visibleRow) const;
    std::size_t totalUnfilteredCount() const noexcept { return m_rawItems.size(); }
    std::size_t visibleCount() const noexcept { return m_filteredIndices.size(); }

    static QString formatFileSize(uint64_t bytes);
    static QString formatCategory(FileCategory category);
    static QString formatTimestamp(uint64_t epochSeconds);

private:
    std::vector<search::SearchResultItem> m_rawItems;
    std::vector<std::size_t>              m_filteredIndices;
    ResultFilterCriteria                  m_criteria;
    int                                   m_sortColumn{ColScore};
    Qt::SortOrder                         m_sortOrder{Qt::DescendingOrder};

    void applyFilterAndSort();
};

} // namespace sse::gui
