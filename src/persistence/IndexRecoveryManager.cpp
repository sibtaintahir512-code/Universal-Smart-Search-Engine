#include "persistence/IndexRecoveryManager.hpp"
#include "persistence/IndexDeserializer.hpp"
#include <fstream>
#include <system_error>

namespace sse::persistence {

namespace {

bool readFileBytes(const std::filesystem::path& path, dsa::CustomDynamicArray<uint8_t>& outBuffer) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    std::streamsize size = file.tellg();
    if (size <= 0 || static_cast<uint64_t>(size) > MAX_INDEX_FILE_SIZE) {
        return false;
    }

    file.seekg(0, std::ios::beg);
    outBuffer.clear();
    outBuffer.reserve(static_cast<std::size_t>(size));

    constexpr std::size_t CHUNK_SIZE = 65536;
    uint8_t chunk[CHUNK_SIZE];
    std::streamsize remaining = size;

    while (remaining > 0 && file.good()) {
        std::streamsize toRead = (remaining < static_cast<std::streamsize>(CHUNK_SIZE)) ? remaining : CHUNK_SIZE;
        file.read(reinterpret_cast<char*>(chunk), toRead);
        std::streamsize bytesRead = file.gcount();
        if (bytesRead <= 0) break;
        for (std::streamsize i = 0; i < bytesRead; ++i) {
            outBuffer.push_back(chunk[i]);
        }
        remaining -= bytesRead;
    }

    return outBuffer.size() == static_cast<std::size_t>(size);
}

} // anonymous namespace

bool IndexRecoveryManager::isolateCorruptedFile(const std::filesystem::path& filePath) {
    std::error_code ec;
    if (!std::filesystem::exists(filePath, ec)) {
        return false;
    }

    std::filesystem::path corruptPath = filePath;
    corruptPath += ".corrupt";

    std::filesystem::remove(corruptPath, ec); // Remove existing .corrupt if present
    std::filesystem::rename(filePath, corruptPath, ec);
    return !ec;
}

PersistenceResult IndexRecoveryManager::recover(
    const std::filesystem::path& indexDirectory,
    const std::string& primaryFileName,
    indexing::DocumentCatalog& outCatalog,
    indexing::InvertedIndex& outIndex) {

    std::filesystem::path primaryPath = indexDirectory / primaryFileName;
    std::filesystem::path backupPath = indexDirectory / (primaryFileName + ".bak");

    std::error_code ec;

    // 1. Attempt loading primary index
    if (std::filesystem::exists(primaryPath, ec)) {
        dsa::CustomDynamicArray<uint8_t> primaryData;
        if (readFileBytes(primaryPath, primaryData)) {
            PersistenceResult res = IndexDeserializer::deserialize(
                primaryData.data(), primaryData.size(), outCatalog, outIndex);

            if (res.isSuccess()) {
                return res;
            }

            // Primary exists but is corrupted
            isolateCorruptedFile(primaryPath);
        } else {
            isolateCorruptedFile(primaryPath);
        }
    }

    // 2. Primary failed or missing: attempt failover to backup index
    if (std::filesystem::exists(backupPath, ec)) {
        dsa::CustomDynamicArray<uint8_t> backupData;
        if (readFileBytes(backupPath, backupData)) {
            PersistenceResult res = IndexDeserializer::deserialize(
                backupData.data(), backupData.size(), outCatalog, outIndex);

            if (res.isSuccess()) {
                // Successfully recovered from backup!
                res.status = PersistenceStatus::RecoveredFromBackup;
                res.message = "Primary index corrupted or missing; successfully recovered from " + backupPath.string();
                return res;
            }
        }
    }

    // 3. Both primary and backup missing or unrecoverable
    if (!std::filesystem::exists(primaryPath, ec) && !std::filesystem::exists(backupPath, ec)) {
        return PersistenceResult::failure(PersistenceStatus::FileNotFound, "No valid index or backup file found");
    }

    return PersistenceResult::failure(PersistenceStatus::RebuildRequired, "All index files are corrupted; complete index rebuild required");
}

} // namespace sse::persistence
