#include "extraction/ExtractionCache.hpp"

namespace sse::extraction {

bool ExtractionCache::get(const std::string& canonicalPath,
                         uint64_t currentSize,
                         uint64_t currentModTime,
                         const std::string& extractorId,
                         const std::string& extractorVersion,
                         ExtractionResult& outResult) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    const CacheEntry* entry = m_entries.find(canonicalPath);
    if (!entry) {
        return false;
    }

    // Invalidation check against composite identity
    if (entry->result.originalFileSize != currentSize ||
        entry->cachedModTime != currentModTime ||
        entry->result.extractorId != extractorId ||
        entry->result.extractorVersion != extractorVersion) {
        return false; // Cache invalidated!
    }

    outResult = entry->result;
    return true;
}

void ExtractionCache::put(const ExtractionResult& result, uint64_t modTime) {
    if (result.sourcePath.empty()) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    CacheEntry entry;
    entry.result = result;
    entry.cachedModTime = modTime;

    m_entries.insert(result.sourcePath, std::move(entry));
}

bool ExtractionCache::invalidate(const std::string& canonicalPath) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    return m_entries.erase(canonicalPath);
}

void ExtractionCache::clear() noexcept {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_entries.clear();
}

std::size_t ExtractionCache::size() const noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_entries.size();
}

} // namespace sse::extraction
