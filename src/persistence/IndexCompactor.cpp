#include "persistence/IndexCompactor.hpp"
#include <chrono>

namespace sse::persistence {

PersistenceResult IndexCompactor::compact(
    const indexing::DocumentCatalog& srcCatalog,
    const indexing::InvertedIndex& srcIndex,
    indexing::DocumentCatalog& outCatalog,
    indexing::InvertedIndex& outIndex,
    const std::atomic<bool>* cancelToken) {

    auto tStart = std::chrono::high_resolution_clock::now();

    if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
        return PersistenceResult::failure(PersistenceStatus::Cancelled, "Compaction cancelled prior to start");
    }

    uint32_t oldTotal = static_cast<uint32_t>(srcCatalog.totalCount());
    uint32_t activeTotal = static_cast<uint32_t>(srcCatalog.activeCount());

    indexing::DocumentCatalog compactedCatalog;
    indexing::InvertedIndex compactedIndex;

    // Pass 1: Build remap table and compact DocumentCatalog array
    dsa::CustomDynamicArray<DocID> remapTable;
    remapTable.reserve(oldTotal + 1);
    for (uint32_t i = 0; i <= oldTotal; ++i) {
        remapTable.push_back(INVALID_DOC_ID);
    }

    DocID newDocId = 1;
    for (DocID oldId = 1; oldId <= oldTotal; ++oldId) {
        if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
            return PersistenceResult::failure(PersistenceStatus::Cancelled, "Compaction cancelled during catalog pass");
        }

        if (srcCatalog.isActive(oldId)) {
            remapTable[oldId] = newDocId;
            const FileMetadata* oldMeta = srcCatalog.getMetadata(oldId);
            if (oldMeta) {
                FileMetadata newMeta = *oldMeta;
                newMeta.docId = newDocId;
                compactedCatalog.registerDocument(newMeta, srcCatalog.getPath(oldId), srcCatalog.getFilename(oldId));
            }
            ++newDocId;
        } else {
            remapTable[oldId] = INVALID_DOC_ID; // Deleted / Tombstoned
        }
    }

    // Pass 2: Rewrite posting lists in InvertedIndex
    uint64_t totalRemappedPostings = 0;
    srcIndex.forEachTerm([&](const std::string& term, const dsa::CustomPostingList& list) {
        const PostingNode* node = list.head();
        while (node) {
            DocID mappedId = (node->docId <= oldTotal) ? remapTable[node->docId] : INVALID_DOC_ID;
            if (mappedId != INVALID_DOC_ID) {
                compactedIndex.restorePosting(term, mappedId, node->nameTermFreq, node->bodyTermFreq, node->fieldMask);
                ++totalRemappedPostings;
            }
            node = node->next;
        }
    });

    // Pass 3: Remap document length statistics
    const auto& oldLengths = srcIndex.documentLengthStatistics();
    for (DocID oldId = 1; oldId <= oldTotal; ++oldId) {
        DocID mappedId = remapTable[oldId];
        if (mappedId != INVALID_DOC_ID) {
            uint32_t nameLen = oldLengths.getNameLength(oldId);
            uint32_t bodyLen = oldLengths.getBodyLength(oldId);
            compactedIndex.documentLengthStatistics().recordDocumentLength(mappedId, nameLen, bodyLen);
        }
    }

    if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
        return PersistenceResult::failure(PersistenceStatus::Cancelled, "Compaction cancelled before commit");
    }

    // Pass 4: Commit
    outCatalog = std::move(compactedCatalog);
    outIndex = std::move(compactedIndex);

    auto tEnd = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    PersistenceResult result = PersistenceResult::success("Compaction completed successfully");
    result.documentCount = activeTotal;
    result.activeDocumentCount = activeTotal;
    result.vocabularySize = static_cast<uint32_t>(outIndex.vocabularySize());
    result.postingsCount = totalRemappedPostings;
    result.durationMs = durationMs;
    return result;
}

} // namespace sse::persistence
