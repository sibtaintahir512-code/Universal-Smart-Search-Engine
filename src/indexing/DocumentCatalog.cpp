#include "indexing/DocumentCatalog.hpp"
#include <algorithm>

namespace sse::indexing {

DocumentCatalog::DocumentCatalog() {
    // Pre-reserve default capacities for string pool and records
    m_records.reserve(64);
    m_stringPool.reserve(4096);
}

DocumentCatalog::DocumentCatalog(DocumentCatalog&& other) noexcept {
    std::unique_lock<std::shared_mutex> lockOther(other.m_mutex);
    m_highestDocId = other.m_highestDocId;
    m_activeCount = other.m_activeCount;
    m_records = std::move(other.m_records);
    m_stringPool = std::move(other.m_stringPool);
    m_pathToDocId = std::move(other.m_pathToDocId);

    other.m_highestDocId = 0;
    other.m_activeCount = 0;
}

DocumentCatalog& DocumentCatalog::operator=(DocumentCatalog&& other) noexcept {
    if (this != &other) {
        std::unique_lock<std::shared_mutex> lockThis(m_mutex, std::defer_lock);
        std::unique_lock<std::shared_mutex> lockOther(other.m_mutex, std::defer_lock);
        std::lock(lockThis, lockOther);

        m_highestDocId = other.m_highestDocId;
        m_activeCount = other.m_activeCount;
        m_records = std::move(other.m_records);
        m_stringPool = std::move(other.m_stringPool);
        m_pathToDocId = std::move(other.m_pathToDocId);

        other.m_highestDocId = 0;
        other.m_activeCount = 0;
    }
    return *this;
}

void DocumentCatalog::appendToStringPool(const std::string& str, uint32_t& outOffset, uint16_t& outLength) {
    outOffset = static_cast<uint32_t>(m_stringPool.size());
    outLength = static_cast<uint16_t>(std::min<std::size_t>(str.size(), 65535));
    for (std::size_t i = 0; i < outLength; ++i) {
        m_stringPool.push_back(str[i]);
    }
}

std::string DocumentCatalog::getStringFromPool(uint32_t offset, uint16_t length) const {
    if (offset + length > m_stringPool.size()) {
        return "";
    }
    return std::string(m_stringPool.data() + offset, length);
}

DocID DocumentCatalog::registerDocument(FileMetadata metadata,
                                       const std::string& canonicalPath,
                                       const std::string& filename) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    // Check if canonical path already registered
    DocID* existingIdPtr = m_pathToDocId.find(canonicalPath);
    if (existingIdPtr && *existingIdPtr != INVALID_DOC_ID) {
        DocID existingId = *existingIdPtr;
        FileMetadata& existingMeta = m_records[existingId - 1];

        // If previously soft-deleted, reactivate it
        if ((existingMeta.flags & META_FLAG_ACTIVE) == 0) {
            existingMeta.flags |= META_FLAG_ACTIVE;
            ++m_activeCount;
        }

        // Compare size and timestamp for incremental modification check
        if (existingMeta.fileSize != metadata.fileSize ||
            existingMeta.lastModifiedTime != metadata.lastModifiedTime) {
            existingMeta.fileSize = metadata.fileSize;
            existingMeta.lastModifiedTime = metadata.lastModifiedTime;
            existingMeta.category = metadata.category;
            existingMeta.extractionState = metadata.extractionState;
            existingMeta.flags |= META_FLAG_MODIFIED;
        }
        return existingId;
    }

    // New document registration: monotonic ID assignment
    DocID newId = ++m_highestDocId;
    metadata.docId = newId;

    // Append strings to string pool and populate offsets
    appendToStringPool(filename, metadata.nameOffset, metadata.nameLength);
    appendToStringPool(canonicalPath, metadata.pathOffset, metadata.pathLength);

    metadata.flags |= META_FLAG_ACTIVE;
    m_records.push_back(metadata);
    m_pathToDocId.insert(canonicalPath, newId);
    ++m_activeCount;

    return newId;
}

const FileMetadata* DocumentCatalog::getMetadata(DocID docId) const noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (docId == INVALID_DOC_ID || docId > m_records.size()) {
        return nullptr;
    }
    return &m_records[docId - 1];
}

FileMetadata* DocumentCatalog::getMutableMetadata(DocID docId) noexcept {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (docId == INVALID_DOC_ID || docId > m_records.size()) {
        return nullptr;
    }
    return &m_records[docId - 1];
}

std::string DocumentCatalog::getPath(DocID docId) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (docId == INVALID_DOC_ID || docId > m_records.size()) {
        return "";
    }
    const FileMetadata& meta = m_records[docId - 1];
    return getStringFromPool(meta.pathOffset, meta.pathLength);
}

std::string DocumentCatalog::getFilename(DocID docId) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (docId == INVALID_DOC_ID || docId > m_records.size()) {
        return "";
    }
    const FileMetadata& meta = m_records[docId - 1];
    return getStringFromPool(meta.nameOffset, meta.nameLength);
}

DocID DocumentCatalog::findDocIdByPath(const std::string& canonicalPath) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    const DocID* idPtr = m_pathToDocId.find(canonicalPath);
    return idPtr ? *idPtr : INVALID_DOC_ID;
}

bool DocumentCatalog::markDeleted(DocID docId) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (docId == INVALID_DOC_ID || docId > m_records.size()) {
        return false;
    }
    FileMetadata& meta = m_records[docId - 1];
    if ((meta.flags & META_FLAG_ACTIVE) == 0) {
        return false;
    }
    meta.flags &= ~META_FLAG_ACTIVE;
    if (m_activeCount > 0) {
        --m_activeCount;
    }
    return true;
}

bool DocumentCatalog::reactivate(DocID docId) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (docId == INVALID_DOC_ID || docId > m_records.size()) {
        return false;
    }
    FileMetadata& meta = m_records[docId - 1];
    if ((meta.flags & META_FLAG_ACTIVE) != 0) {
        return false;
    }
    meta.flags |= META_FLAG_ACTIVE;
    ++m_activeCount;
    return true;
}

bool DocumentCatalog::isActive(DocID docId) const noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (docId == INVALID_DOC_ID || docId > m_records.size()) {
        return false;
    }
    return (m_records[docId - 1].flags & META_FLAG_ACTIVE) != 0;
}

std::size_t DocumentCatalog::totalCount() const noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_records.size();
}

std::size_t DocumentCatalog::activeCount() const noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_activeCount;
}

void DocumentCatalog::clear() noexcept {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_records.clear();
    m_stringPool.clear();
    m_pathToDocId.clear();
    m_highestDocId = 0;
    m_activeCount = 0;
}

} // namespace sse::indexing
