#include "search/ResultSet.hpp"

namespace sse::search {

ResultSet::ResultSet(size_type initialCapacity)
    : m_docIds(initialCapacity) {}

void ResultSet::add(DocID docId) {
    if (docId == INVALID_DOC_ID) {
        return;
    }

    if (m_docIds.empty() || docId > m_docIds.back()) {
        m_docIds.push_back(docId);
        return;
    }

    if (docId == m_docIds.back()) {
        return; // Duplicate
    }

    // Binary search for insertion point
    std::size_t low = 0;
    std::size_t high = m_docIds.size();
    while (low < high) {
        std::size_t mid = low + (high - low) / 2;
        if (m_docIds[mid] == docId) {
            return; // Duplicate
        }
        if (m_docIds[mid] < docId) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    // Insert at index low
    m_docIds.insert(low, docId);
}

bool ResultSet::contains(DocID docId) const noexcept {
    if (m_docIds.empty() || docId == INVALID_DOC_ID) {
        return false;
    }

    std::size_t low = 0;
    std::size_t high = m_docIds.size();
    while (low < high) {
        std::size_t mid = low + (high - low) / 2;
        if (m_docIds[mid] == docId) {
            return true;
        }
        if (m_docIds[mid] < docId) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return false;
}

ResultSet ResultSet::intersect(const ResultSet& a, const ResultSet& b) {
    ResultSet result;
    if (a.empty() || b.empty()) {
        return result;
    }

    std::size_t i = 0;
    std::size_t j = 0;
    const std::size_t sizeA = a.size();
    const std::size_t sizeB = b.size();

    while (i < sizeA && j < sizeB) {
        DocID idA = a[i];
        DocID idB = b[j];

        if (idA == idB) {
            result.m_docIds.push_back(idA);
            ++i;
            ++j;
        } else if (idA < idB) {
            ++i;
        } else {
            ++j;
        }
    }

    return result;
}

ResultSet ResultSet::unionSets(const ResultSet& a, const ResultSet& b) {
    ResultSet result;
    std::size_t i = 0;
    std::size_t j = 0;
    const std::size_t sizeA = a.size();
    const std::size_t sizeB = b.size();

    while (i < sizeA && j < sizeB) {
        DocID idA = a[i];
        DocID idB = b[j];

        if (idA == idB) {
            result.m_docIds.push_back(idA);
            ++i;
            ++j;
        } else if (idA < idB) {
            result.m_docIds.push_back(idA);
            ++i;
        } else {
            result.m_docIds.push_back(idB);
            ++j;
        }
    }

    while (i < sizeA) {
        result.m_docIds.push_back(a[i]);
        ++i;
    }

    while (j < sizeB) {
        result.m_docIds.push_back(b[j]);
        ++j;
    }

    return result;
}

ResultSet ResultSet::difference(const ResultSet& universe, const ResultSet& exclude) {
    ResultSet result;
    if (universe.empty()) {
        return result;
    }
    if (exclude.empty()) {
        return universe;
    }

    std::size_t i = 0;
    std::size_t j = 0;
    const std::size_t sizeU = universe.size();
    const std::size_t sizeE = exclude.size();

    while (i < sizeU && j < sizeE) {
        DocID idU = universe[i];
        DocID idE = exclude[j];

        if (idU == idE) {
            ++i;
            ++j;
        } else if (idU < idE) {
            result.m_docIds.push_back(idU);
            ++i;
        } else {
            ++j;
        }
    }

    while (i < sizeU) {
        result.m_docIds.push_back(universe[i]);
        ++i;
    }

    return result;
}

} // namespace sse::search
