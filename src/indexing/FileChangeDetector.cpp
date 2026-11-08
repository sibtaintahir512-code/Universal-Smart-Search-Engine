#include "indexing/FileChangeDetector.hpp"
#include "dsa/CustomHashTable.hpp"
#include <filesystem>

namespace sse::indexing {

FileChangeStatus FileChangeDetector::detectChange(const DocumentCatalog& catalog,
                                                 const std::string& canonicalPath,
                                                 uint64_t currentSize,
                                                 uint64_t currentModTime,
                                                 bool existsOnDisk) noexcept {
    DocID docId = catalog.findDocIdByPath(canonicalPath);

    if (docId == INVALID_DOC_ID) {
        if (!existsOnDisk) {
            return FileChangeStatus::Error;
        }
        return FileChangeStatus::New;
    }

    if (!existsOnDisk) {
        return FileChangeStatus::Missing;
    }

    const FileMetadata* storedMeta = catalog.getMetadata(docId);
    if (!storedMeta) {
        return FileChangeStatus::Error;
    }

    // If previously tombstoned and now exists, it is treated as a reactivated/new document
    if ((storedMeta->flags & META_FLAG_ACTIVE) == 0) {
        return FileChangeStatus::New;
    }

    // Fast O(1) attribute check
    if (storedMeta->fileSize != currentSize || storedMeta->lastModifiedTime != currentModTime) {
        return FileChangeStatus::Modified;
    }

    return FileChangeStatus::Unchanged;
}

std::size_t FileChangeDetector::reconcileMissingDocuments(DocumentCatalog& catalog,
                                                         const dsa::CustomDynamicArray<std::string>& discoveredPaths) {
    // Build fast lookup table of discovered paths
    dsa::CustomHashTable<std::string, bool> discoveredTable(discoveredPaths.size() * 2 + 16);
    for (std::size_t i = 0; i < discoveredPaths.size(); ++i) {
        discoveredTable.insert(discoveredPaths[i], true);
    }

    std::size_t missingCount = 0;
    const std::size_t total = catalog.totalCount();

    for (DocID id = 1; id <= static_cast<DocID>(total); ++id) {
        if (catalog.isActive(id)) {
            std::string path = catalog.getPath(id);
            if (!discoveredTable.contains(path)) {
                // Secondary safety check: verify path does not exist on disk
                std::error_code ec;
                if (!std::filesystem::exists(std::filesystem::u8path(path), ec)) {
                    catalog.markDeleted(id);
                    ++missingCount;
                }
            }
        }
    }

    return missingCount;
}

} // namespace sse::indexing
