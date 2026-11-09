#include "persistence/IndexPersistenceManager.hpp"
#include "persistence/IndexSerializer.hpp"
#include "persistence/IndexDeserializer.hpp"
#include "persistence/IndexCompactor.hpp"
#include "persistence/IndexRecoveryManager.hpp"
#include <fstream>
#include <chrono>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif

namespace sse::persistence {

namespace {

bool flushAndSync(std::ofstream& stream, const std::filesystem::path& path) {
    stream.flush();
    if (!stream.good()) return false;

#ifdef _WIN32
    HANDLE hFile = CreateFileW(path.wstring().c_str(), GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(hFile);
        CloseHandle(hFile);
    }
#else
    int fd = open(path.c_str(), O_WRONLY);
    if (fd >= 0) {
        fsync(fd);
        close(fd);
    }
#endif
    return true;
}

bool readEntireFile(const std::filesystem::path& path, dsa::CustomDynamicArray<uint8_t>& outBuffer) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize size = file.tellg();
    if (size <= 0 || static_cast<uint64_t>(size) > MAX_INDEX_FILE_SIZE) return false;

    file.seekg(0, std::ios::beg);
    outBuffer.clear();
    outBuffer.reserve(static_cast<std::size_t>(size));

    constexpr std::size_t CHUNK = 65536;
    uint8_t chunk[CHUNK];
    std::streamsize rem = size;

    while (rem > 0 && file.good()) {
        std::streamsize toRead = (rem < static_cast<std::streamsize>(CHUNK)) ? rem : CHUNK;
        file.read(reinterpret_cast<char*>(chunk), toRead);
        std::streamsize readBytes = file.gcount();
        if (readBytes <= 0) break;
        for (std::streamsize i = 0; i < readBytes; ++i) {
            outBuffer.push_back(chunk[i]);
        }
        rem -= readBytes;
    }

    return outBuffer.size() == static_cast<std::size_t>(size);
}

} // anonymous namespace

IndexPersistenceManager::IndexPersistenceManager(
    indexing::InvertedIndex* index,
    indexing::DocumentCatalog* catalog,
    std::filesystem::path indexDirectory)
    : m_index(index), m_catalog(catalog), m_indexDirectory(std::move(indexDirectory)) {
}

std::filesystem::path IndexPersistenceManager::resolveFilePath(const std::string& customFileName) const {
    std::string name = customFileName.empty() ? "search.idx" : customFileName;
    return m_indexDirectory / name;
}

PersistenceResult IndexPersistenceManager::saveIndex(const std::string& customFileName) {
    if (!m_index || !m_catalog) {
        return PersistenceResult::failure(PersistenceStatus::InvalidState, "Index or catalog pointer is null");
    }

    auto tStart = std::chrono::high_resolution_clock::now();

    std::error_code ec;
    std::filesystem::create_directories(m_indexDirectory, ec);
    if (ec) {
        return PersistenceResult::failure(PersistenceStatus::WriteError, "Failed creating index directory: " + ec.message());
    }

    std::filesystem::path targetPath = resolveFilePath(customFileName);
    std::filesystem::path tempPath = targetPath;
    tempPath += ".tmp";
    std::filesystem::path backupPath = targetPath;
    backupPath += ".bak";

    // 1. Serialize in-memory index to binary writer
    BinaryWriter writer;
    PersistenceResult serRes = IndexSerializer::serialize(*m_catalog, *m_index, writer, m_generation, m_cancelToken);
    if (!serRes.isSuccess()) {
        return serRes;
    }

    // 2. Write to temporary file
    {
        std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            return PersistenceResult::failure(PersistenceStatus::WriteError, "Failed opening temporary file for write: " + tempPath.string());
        }

        file.write(reinterpret_cast<const char*>(writer.data()), static_cast<std::streamsize>(writer.size()));
        if (!file.good()) {
            file.close();
            std::filesystem::remove(tempPath, ec);
            return PersistenceResult::failure(PersistenceStatus::WriteError, "Failed writing bytes to temporary index file");
        }

        // 3. Flush and synchronize disk buffers
        if (!flushAndSync(file, tempPath)) {
            file.close();
            std::filesystem::remove(tempPath, ec);
            return PersistenceResult::failure(PersistenceStatus::WriteError, "Failed flushing index buffers to disk");
        }
    }

    // 4. Verification pass: read back and validate temporary file
    dsa::CustomDynamicArray<uint8_t> readBackData;
    if (!readEntireFile(tempPath, readBackData)) {
        std::filesystem::remove(tempPath, ec);
        return PersistenceResult::failure(PersistenceStatus::ReadError, "Failed reading back temporary file for verification");
    }

    PersistenceResult valRes = IndexDeserializer::validate(readBackData.data(), readBackData.size());
    if (!valRes.isSuccess()) {
        std::filesystem::remove(tempPath, ec);
        return PersistenceResult::failure(valRes.status, "Temporary index failed post-write validation: " + valRes.message);
    }

    // 5. Preserve previous target as backup
    if (std::filesystem::exists(targetPath, ec)) {
        std::filesystem::copy_file(targetPath, backupPath, std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(targetPath, ec);
    }

    // 6. Atomic replacement
    std::filesystem::rename(tempPath, targetPath, ec);
    if (ec) {
        // Attempt recovery from backup if rename failed
        if (std::filesystem::exists(backupPath, ec)) {
            std::filesystem::rename(backupPath, targetPath, ec);
        }
        return PersistenceResult::failure(PersistenceStatus::WriteError, "Atomic rename of temporary index failed: " + ec.message());
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    m_statistics.recordSave(durationMs, writer.size());
    ++m_generation;

    PersistenceResult result = PersistenceResult::success("Index saved successfully to " + targetPath.string());
    result.bytesProcessed = writer.size();
    result.documentCount = serRes.documentCount;
    result.activeDocumentCount = serRes.activeDocumentCount;
    result.vocabularySize = serRes.vocabularySize;
    result.postingsCount = serRes.postingsCount;
    result.durationMs = durationMs;
    result.generation = m_generation - 1;
    return result;
}

PersistenceResult IndexPersistenceManager::loadIndex(const std::string& customFileName) {
    if (!m_index || !m_catalog) {
        return PersistenceResult::failure(PersistenceStatus::InvalidState, "Index or catalog pointer is null");
    }

    std::filesystem::path targetPath = resolveFilePath(customFileName);
    std::error_code ec;

    if (!std::filesystem::exists(targetPath, ec)) {
        return PersistenceResult::failure(PersistenceStatus::FileNotFound, "Index file not found: " + targetPath.string());
    }

    auto tStart = std::chrono::high_resolution_clock::now();

    dsa::CustomDynamicArray<uint8_t> data;
    if (!readEntireFile(targetPath, data)) {
        return PersistenceResult::failure(PersistenceStatus::ReadError, "Failed reading index file from disk: " + targetPath.string());
    }

    PersistenceResult desRes = IndexDeserializer::deserialize(
        data.data(), data.size(), *m_catalog, *m_index, m_cancelToken);

    if (desRes.isSuccess()) {
        auto tEnd = std::chrono::high_resolution_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        m_statistics.recordLoad(durationMs, data.size());
        m_generation = desRes.generation + 1;
    }

    return desRes;
}

PersistenceResult IndexPersistenceManager::validateIndex(const std::string& customFileName) {
    std::filesystem::path targetPath = resolveFilePath(customFileName);
    std::error_code ec;

    if (!std::filesystem::exists(targetPath, ec)) {
        return PersistenceResult::failure(PersistenceStatus::FileNotFound, "Index file not found: " + targetPath.string());
    }

    dsa::CustomDynamicArray<uint8_t> data;
    if (!readEntireFile(targetPath, data)) {
        return PersistenceResult::failure(PersistenceStatus::ReadError, "Failed reading index file from disk");
    }

    return IndexDeserializer::validate(data.data(), data.size());
}

PersistenceResult IndexPersistenceManager::recoverIndex(const std::string& customFileName) {
    if (!m_index || !m_catalog) {
        return PersistenceResult::failure(PersistenceStatus::InvalidState, "Index or catalog pointer is null");
    }

    std::string baseName = customFileName.empty() ? "search.idx" : customFileName;
    PersistenceResult res = IndexRecoveryManager::recover(m_indexDirectory, baseName, *m_catalog, *m_index);

    if (res.isSuccess()) {
        m_statistics.recordRecovery();
        m_generation = res.generation + 1;
    }

    return res;
}

PersistenceResult IndexPersistenceManager::compactIndex() {
    if (!m_index || !m_catalog) {
        return PersistenceResult::failure(PersistenceStatus::InvalidState, "Index or catalog pointer is null");
    }

    auto tStart = std::chrono::high_resolution_clock::now();

    indexing::DocumentCatalog compactedCatalog;
    indexing::InvertedIndex compactedIndex;

    PersistenceResult compRes = IndexCompactor::compact(
        *m_catalog, *m_index, compactedCatalog, compactedIndex, m_cancelToken);

    if (!compRes.isSuccess()) {
        return compRes;
    }

    // Atomically commit compacted state
    *m_catalog = std::move(compactedCatalog);
    *m_index = std::move(compactedIndex);

    auto tEnd = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    m_statistics.recordCompaction(durationMs);

    return compRes;
}

} // namespace sse::persistence
