#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include "persistence/IndexFileFormat.hpp"
#include "persistence/PersistenceResult.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"

namespace sse::persistence {

/**
 * @brief High-throughput binary serializer for DocumentCatalog and InvertedIndex.
 */
class IndexSerializer {
public:
    IndexSerializer() = default;

    /**
     * @brief Serializes the active catalog and inverted index into an in-memory binary snapshot.
     *
     * @param catalog Source DocumentCatalog.
     * @param index Source InvertedIndex.
     * @param outWriter Output BinaryWriter receiving the complete validated binary index.
     * @param generation Monotonic snapshot generation number.
     * @param cancelToken Optional cancellation flag.
     * @return PersistenceResult Detailed operation telemetry and status.
     */
    static PersistenceResult serialize(
        const indexing::DocumentCatalog& catalog,
        const indexing::InvertedIndex& index,
        BinaryWriter& outWriter,
        uint64_t generation = 1,
        const std::atomic<bool>* cancelToken = nullptr);
};

} // namespace sse::persistence
