#pragma once

#include <filesystem>
#include <string>
#include "persistence/PersistenceResult.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"

namespace sse::persistence {

/**
 * @brief Crash recovery, corruption isolation, and backup failover manager.
 */
class IndexRecoveryManager {
public:
    IndexRecoveryManager() = default;

    /**
     * @brief Evaluates primary index, backup index, and temporary artifacts to restore a valid state.
     *
     * @param indexDirectory Directory containing index artifacts.
     * @param primaryFileName Base filename (e.g. "search.idx").
     * @param outCatalog Output catalog receiving restored documents.
     * @param outIndex Output inverted index receiving restored postings.
     * @return PersistenceResult Detailed recovery diagnostic.
     */
    static PersistenceResult recover(
        const std::filesystem::path& indexDirectory,
        const std::string& primaryFileName,
        indexing::DocumentCatalog& outCatalog,
        indexing::InvertedIndex& outIndex);

    /**
     * @brief Isolates a corrupted file by renaming it to filename.corrupt.
     */
    static bool isolateCorruptedFile(const std::filesystem::path& filePath);
};

} // namespace sse::persistence
