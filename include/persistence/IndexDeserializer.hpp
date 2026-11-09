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
 * @brief Memory-safe, untrusted-input resilient binary deserializer.
 *
 * Implements strict multi-stage validation:
 * 1. File-level CRC32 checksum verification.
 * 2. Magic signature and endianness confirmation.
 * 3. Section directory bounds and per-section CRC32 verification.
 * 4. Staged deserialization into temporary structures to protect live state.
 * 5. Atomic state replacement upon complete success.
 */
class IndexDeserializer {
public:
    IndexDeserializer() = default;

    /**
     * @brief Deserializes a binary index buffer and populates outCatalog and outIndex.
     *
     * @param data Pointer to raw in-memory index file bytes.
     * @param size Size in bytes of the index buffer.
     * @param outCatalog Output catalog receiving restored documents.
     * @param outIndex Output inverted index receiving restored vocabulary, postings, and lengths.
     * @param cancelToken Optional cooperative cancellation flag.
     * @return PersistenceResult Detailed operation telemetry and error diagnostic.
     */
    static PersistenceResult deserialize(
        const uint8_t* data,
        std::size_t size,
        indexing::DocumentCatalog& outCatalog,
        indexing::InvertedIndex& outIndex,
        const std::atomic<bool>* cancelToken = nullptr);

    /**
     * @brief Performs dry-run structural and checksum validation without mutating catalog/index.
     */
    static PersistenceResult validate(const uint8_t* data, std::size_t size);
};

} // namespace sse::persistence
