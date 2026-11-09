#include "ranking/DocumentLengthStatistics.hpp"

namespace sse::ranking {

DocumentLengthStatistics::DocumentLengthStatistics(DocumentLengthStatistics&& other) noexcept {
    std::unique_lock<std::shared_mutex> lock(other.m_mutex);
    m_lengths = std::move(other.m_lengths);
    m_totalNameTokens = other.m_totalNameTokens;
    m_totalBodyTokens = other.m_totalBodyTokens;
    m_activeDocs = other.m_activeDocs;

    other.m_totalNameTokens = 0;
    other.m_totalBodyTokens = 0;
    other.m_activeDocs = 0;
}

DocumentLengthStatistics& DocumentLengthStatistics::operator=(DocumentLengthStatistics&& other) noexcept {
    if (this != &other) {
        std::unique_lock<std::shared_mutex> lockThis(m_mutex, std::defer_lock);
        std::unique_lock<std::shared_mutex> lockOther(other.m_mutex, std::defer_lock);
        std::lock(lockThis, lockOther);

        m_lengths = std::move(other.m_lengths);
        m_totalNameTokens = other.m_totalNameTokens;
        m_totalBodyTokens = other.m_totalBodyTokens;
        m_activeDocs = other.m_activeDocs;

        other.m_totalNameTokens = 0;
        other.m_totalBodyTokens = 0;
        other.m_activeDocs = 0;
    }
    return *this;
}

void DocumentLengthStatistics::recordDocumentLength(DocID docId, uint32_t nameTokens, uint32_t bodyTokens) {
    if (docId == INVALID_DOC_ID) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);

    std::size_t targetIndex = static_cast<std::size_t>(docId - 1);
    if (targetIndex >= m_lengths.size()) {
        DocLengthEntry emptyEntry;
        while (m_lengths.size() <= targetIndex) {
            m_lengths.push_back(emptyEntry);
        }
    }

    DocLengthEntry& entry = m_lengths[targetIndex];
    if (entry.isActive) {
        // Update existing document: subtract previous totals
        m_totalNameTokens -= entry.nameTokens;
        m_totalBodyTokens -= entry.bodyTokens;
    } else {
        entry.isActive = true;
        ++m_activeDocs;
    }

    entry.nameTokens = nameTokens;
    entry.bodyTokens = bodyTokens;
    m_totalNameTokens += nameTokens;
    m_totalBodyTokens += bodyTokens;
}

void DocumentLengthStatistics::accumulateDocumentLength(DocID docId, uint32_t nameFreq, uint32_t bodyFreq) {
    if (docId == INVALID_DOC_ID || (nameFreq == 0 && bodyFreq == 0)) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);

    std::size_t targetIndex = static_cast<std::size_t>(docId - 1);
    if (targetIndex >= m_lengths.size()) {
        DocLengthEntry emptyEntry;
        while (m_lengths.size() <= targetIndex) {
            m_lengths.push_back(emptyEntry);
        }
    }

    DocLengthEntry& entry = m_lengths[targetIndex];
    if (!entry.isActive) {
        entry.isActive = true;
        ++m_activeDocs;
    }

    entry.nameTokens += nameFreq;
    entry.bodyTokens += bodyFreq;
    m_totalNameTokens += nameFreq;
    m_totalBodyTokens += bodyFreq;
}

bool DocumentLengthStatistics::removeDocument(DocID docId) {
    if (docId == INVALID_DOC_ID) return false;

    std::unique_lock<std::shared_mutex> lock(m_mutex);

    std::size_t targetIndex = static_cast<std::size_t>(docId - 1);
    if (targetIndex >= m_lengths.size() || !m_lengths[targetIndex].isActive) {
        return false;
    }

    DocLengthEntry& entry = m_lengths[targetIndex];
    m_totalNameTokens -= entry.nameTokens;
    m_totalBodyTokens -= entry.bodyTokens;
    entry.nameTokens = 0;
    entry.bodyTokens = 0;
    entry.isActive = false;

    if (m_activeDocs > 0) {
        --m_activeDocs;
    }

    return true;
}

void DocumentLengthStatistics::clear() noexcept {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_lengths.clear();
    m_totalNameTokens = 0;
    m_totalBodyTokens = 0;
    m_activeDocs = 0;
}

uint32_t DocumentLengthStatistics::getNameLength(DocID docId) const noexcept {
    if (docId == INVALID_DOC_ID) return 0;
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    std::size_t idx = static_cast<std::size_t>(docId - 1);
    if (idx < m_lengths.size() && m_lengths[idx].isActive) {
        return m_lengths[idx].nameTokens;
    }
    return 0;
}

uint32_t DocumentLengthStatistics::getBodyLength(DocID docId) const noexcept {
    if (docId == INVALID_DOC_ID) return 0;
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    std::size_t idx = static_cast<std::size_t>(docId - 1);
    if (idx < m_lengths.size() && m_lengths[idx].isActive) {
        return m_lengths[idx].bodyTokens;
    }
    return 0;
}

uint32_t DocumentLengthStatistics::getTotalLength(DocID docId) const noexcept {
    if (docId == INVALID_DOC_ID) return 0;
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    std::size_t idx = static_cast<std::size_t>(docId - 1);
    if (idx < m_lengths.size() && m_lengths[idx].isActive) {
        return m_lengths[idx].nameTokens + m_lengths[idx].bodyTokens;
    }
    return 0;
}

bool DocumentLengthStatistics::hasDocument(DocID docId) const noexcept {
    if (docId == INVALID_DOC_ID) return false;
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    std::size_t idx = static_cast<std::size_t>(docId - 1);
    return (idx < m_lengths.size() && m_lengths[idx].isActive);
}

std::size_t DocumentLengthStatistics::activeDocumentCount() const noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_activeDocs;
}

uint64_t DocumentLengthStatistics::totalNameTokens() const noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_totalNameTokens;
}

uint64_t DocumentLengthStatistics::totalBodyTokens() const noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_totalBodyTokens;
}

double DocumentLengthStatistics::averageNameLength() const noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (m_activeDocs == 0) return 0.0;
    return static_cast<double>(m_totalNameTokens) / static_cast<double>(m_activeDocs);
}

double DocumentLengthStatistics::averageBodyLength() const noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (m_activeDocs == 0) return 0.0;
    return static_cast<double>(m_totalBodyTokens) / static_cast<double>(m_activeDocs);
}

double DocumentLengthStatistics::averageTotalLength() const noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (m_activeDocs == 0) return 0.0;
    return static_cast<double>(m_totalNameTokens + m_totalBodyTokens) / static_cast<double>(m_activeDocs);
}

} // namespace sse::ranking
