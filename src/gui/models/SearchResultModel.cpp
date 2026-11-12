#include "gui/models/SearchResultModel.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace sse::gui {

bool ResultFilterCriteria::matches(const search::SearchResultItem& item) const {
    // Category check
    switch (item.category) {
        case FileCategory::Documents:
            if (!includeDocuments) return false;
            break;
        case FileCategory::SourceCode:
            if (!includeCode) return false;
            break;
        case FileCategory::Images:
            if (!includeImages) return false;
            break;
        case FileCategory::Audio:
        case FileCategory::Videos:
            if (!includeMedia) return false;
            break;
        case FileCategory::Archives:
            if (!includeArchives) return false;
            break;
        case FileCategory::Executables:
        case FileCategory::Other:
        default:
            if (!includeOther) return false;
            break;
    }

    // Size check
    if (item.fileSize < minSizeBytes || item.fileSize > maxSizeBytes) {
        return false;
    }

    // Date check
    if (minModifiedTimestamp > 0 && item.lastModified < minModifiedTimestamp) {
        return false;
    }

    return true;
}

SearchResultModel::SearchResultModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int SearchResultModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_filteredIndices.size());
}

int SearchResultModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant SearchResultModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || static_cast<std::size_t>(index.row()) >= m_filteredIndices.size()) {
        return QVariant();
    }

    const auto& item = m_rawItems[m_filteredIndices[static_cast<std::size_t>(index.row())]];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case ColCategory:
                return formatCategory(item.category);
            case ColFilename:
                return QString::fromStdString(item.filename);
            case ColScore: {
                if (item.relevanceScore > 0.0) {
                    return QString::number(item.relevanceScore, 'f', 2);
                }
                return QString("#%1").arg(item.rankPosition);
            }
            case ColSize:
                return formatFileSize(item.fileSize);
            case ColModified:
                return formatTimestamp(item.lastModified);
            case ColPath:
                return QString::fromStdString(item.path);
            default:
                return QVariant();
        }
    } else if (role == Qt::ToolTipRole) {
        return QString("Path: %1\nSize: %2\nModified: %3\nScore: %4")
            .arg(QString::fromStdString(item.path))
            .arg(formatFileSize(item.fileSize))
            .arg(formatTimestamp(item.lastModified))
            .arg(QString::number(item.relevanceScore, 'f', 4));
    } else if (role == DocIdRole) {
        return QVariant::fromValue(static_cast<quint32>(item.docId));
    } else if (role == RawScoreRole) {
        return item.relevanceScore;
    } else if (role == RawSizeRole) {
        return static_cast<qulonglong>(item.fileSize);
    } else if (role == RawTimestampRole) {
        return static_cast<qulonglong>(item.lastModified);
    } else if (role == CategoryRole) {
        return static_cast<int>(item.category);
    } else if (role == CanonicalPathRole) {
        return QString::fromStdString(item.path);
    }

    return QVariant();
}

QVariant SearchResultModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case ColCategory: return "Type";
            case ColFilename: return "File Name";
            case ColScore:    return "Relevance";
            case ColSize:     return "Size";
            case ColModified: return "Modified Date";
            case ColPath:     return "Path";
            default:          return QVariant();
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

void SearchResultModel::sort(int column, Qt::SortOrder order) {
    m_sortColumn = column;
    m_sortOrder = order;
    beginResetModel();
    applyFilterAndSort();
    endResetModel();
}

void SearchResultModel::setSearchResponse(const search::SearchResponse& response) {
    beginResetModel();
    m_rawItems.clear();
    m_rawItems.reserve(response.results.size());
    for (std::size_t i = 0; i < response.results.size(); ++i) {
        m_rawItems.push_back(response.results[i]);
    }
    applyFilterAndSort();
    endResetModel();
}

void SearchResultModel::clear() {
    beginResetModel();
    m_rawItems.clear();
    m_filteredIndices.clear();
    endResetModel();
}

void SearchResultModel::setFilterCriteria(const ResultFilterCriteria& criteria) {
    beginResetModel();
    m_criteria = criteria;
    applyFilterAndSort();
    endResetModel();
}

const search::SearchResultItem* SearchResultModel::getItem(int visibleRow) const {
    if (visibleRow < 0 || static_cast<std::size_t>(visibleRow) >= m_filteredIndices.size()) {
        return nullptr;
    }
    return &m_rawItems[m_filteredIndices[static_cast<std::size_t>(visibleRow)]];
}

void SearchResultModel::applyFilterAndSort() {
    m_filteredIndices.clear();
    for (std::size_t i = 0; i < m_rawItems.size(); ++i) {
        if (m_criteria.matches(m_rawItems[i])) {
            m_filteredIndices.push_back(i);
        }
    }

    auto comparator = [this](std::size_t a, std::size_t b) -> bool {
        const auto& itemA = m_rawItems[a];
        const auto& itemB = m_rawItems[b];

        bool less = false;
        switch (m_sortColumn) {
            case ColCategory:
                less = static_cast<int>(itemA.category) < static_cast<int>(itemB.category);
                break;
            case ColFilename:
                less = QString::compare(QString::fromStdString(itemA.filename),
                                        QString::fromStdString(itemB.filename),
                                        Qt::CaseInsensitive) < 0;
                break;
            case ColScore:
                less = itemA.relevanceScore < itemB.relevanceScore;
                break;
            case ColSize:
                less = itemA.fileSize < itemB.fileSize;
                break;
            case ColModified:
                less = itemA.lastModified < itemB.lastModified;
                break;
            case ColPath:
                less = itemA.path < itemB.path;
                break;
            default:
                less = itemA.rankPosition < itemB.rankPosition;
                break;
        }

        return (m_sortOrder == Qt::AscendingOrder) ? less : !less;
    };

    std::sort(m_filteredIndices.begin(), m_filteredIndices.end(), comparator);
}

QString SearchResultModel::formatFileSize(uint64_t bytes) {
    if (bytes < 1024ULL) {
        return QString("%1 B").arg(bytes);
    } else if (bytes < 1024ULL * 1024ULL) {
        return QString("%1 KB").arg(QString::number(static_cast<double>(bytes) / 1024.0, 'f', 1));
    } else if (bytes < 1024ULL * 1024ULL * 1024ULL) {
        return QString("%1 MB").arg(QString::number(static_cast<double>(bytes) / (1024.0 * 1024.0), 'f', 2));
    } else {
        return QString("%1 GB").arg(QString::number(static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0), 'f', 2));
    }
}

QString SearchResultModel::formatCategory(FileCategory category) {
    switch (category) {
        case FileCategory::Documents:   return "Document";
        case FileCategory::SourceCode:  return "Code";
        case FileCategory::Images:      return "Image";
        case FileCategory::Audio:       return "Audio";
        case FileCategory::Videos:      return "Video";
        case FileCategory::Archives:    return "Archive";
        case FileCategory::Executables: return "Executable";
        case FileCategory::Other:
        default:                        return "Other";
    }
}

QString SearchResultModel::formatTimestamp(uint64_t epochSeconds) {
    if (epochSeconds == 0) {
        return "Unknown";
    }
    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(epochSeconds)).toString("yyyy-MM-dd hh:mm");
}

} // namespace sse::gui
