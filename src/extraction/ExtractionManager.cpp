#include "extraction/ExtractionManager.hpp"
#include "extraction/TextExtractor.hpp"
#include "extraction/SourceCodeExtractor.hpp"
#include "extraction/OfficeExtractor.hpp"
#include "extraction/PDFExtractor.hpp"
#include "extraction/ImageMetadataExtractor.hpp"
#include "extraction/MediaMetadataExtractor.hpp"
#include "extraction/ArchiveMetadataExtractor.hpp"
#include "indexing/FileMetadataManager.hpp"
#include <algorithm>
#include <filesystem>
#include <chrono>

namespace sse::extraction {

ExtractionManager::ExtractionManager(uint64_t maxExtractionBytes) {
    setMaxExtractionBytes(maxExtractionBytes);
    registerDefaultExtractors();
}

void ExtractionManager::registerDefaultExtractors() {
    registerExtractor(std::make_shared<TextExtractor>());
    registerExtractor(std::make_shared<SourceCodeExtractor>());
    registerExtractor(std::make_shared<OfficeExtractor>());
    registerExtractor(std::make_shared<PDFExtractor>());
    registerExtractor(std::make_shared<ImageMetadataExtractor>());
    registerExtractor(std::make_shared<MediaMetadataExtractor>());
    registerExtractor(std::make_shared<ArchiveMetadataExtractor>());
}

void ExtractionManager::registerExtractor(std::shared_ptr<IContentExtractor> extractor) {
    if (!extractor) return;
    std::unique_lock<std::shared_mutex> lock(m_extractorMutex);
    m_extractors.push_back(std::move(extractor));
}

void ExtractionManager::setMaxExtractionBytes(uint64_t maxBytes) noexcept {
    constexpr uint64_t MIN_CAP = 5ULL * 1024 * 1024;    // 5 MB
    constexpr uint64_t MAX_CAP = 100ULL * 1024 * 1024;  // 100 MB
    m_maxExtractionBytes = std::clamp(maxBytes, MIN_CAP, MAX_CAP);
}

IContentExtractor* ExtractionManager::findExtractor(const std::string& extension, FileCategory category) const noexcept {
    std::shared_lock<std::shared_mutex> lock(m_extractorMutex);

    std::string normExt = indexing::FileMetadataManager::normalizeExtension(extension);

    // 1. Try extension match first
    for (const auto& extractor : m_extractors) {
        if (extractor->supportsExtension(normExt)) {
            return extractor.get();
        }
    }

    // 2. Try category match as fallback
    for (const auto& extractor : m_extractors) {
        if (extractor->supportsCategory(category)) {
            return extractor.get();
        }
    }

    return nullptr;
}

ExtractionResult ExtractionManager::extract(const std::string& filePath,
                                           FileCategory category,
                                           DocID docId,
                                           const std::atomic<bool>* cancelToken) {
    auto tStart = std::chrono::steady_clock::now();

    ExtractionResult result;
    result.sourcePath = filePath;
    result.docId = docId;

    if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
        result.status = ExtractionStatus::Cancelled;
        result.errorDescription = "Extraction cancelled before start";
        return result;
    }

    std::error_code ec;
    auto fsPath = std::filesystem::u8path(filePath);
    if (!std::filesystem::exists(fsPath, ec) || ec) {
        result.status = ExtractionStatus::Failed;
        result.errorDescription = "Target file does not exist";
        return result;
    }

    uint64_t fileSize = std::filesystem::file_size(fsPath, ec);
    if (ec) {
        result.status = ExtractionStatus::PermissionDenied;
        result.errorDescription = "Failed to query file size";
        return result;
    }
    result.originalFileSize = fileSize;

    // 1 GB Release 1 cutoff invariant
    if (fileSize > ABSOLUTE_MAX_CUTOFF) {
        result.status = ExtractionStatus::MetadataOnly;
        result.errorDescription = "File size exceeds 1 GB threshold; restricted to metadata-only in Release 1";
        auto tEnd = std::chrono::steady_clock::now();
        result.durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        return result;
    }

    // Query canonical path and modification time for caching
    auto canonPath = std::filesystem::weakly_canonical(fsPath, ec).u8string();
    if (ec) canonPath = filePath;

    auto lastWrite = std::filesystem::last_write_time(fsPath, ec);
    uint64_t modTime = ec ? 0ULL : indexing::FileMetadataManager::fileTimeToEpoch(lastWrite);

    // Determine extension and category
    std::string ext = fsPath.extension().u8string();
    if (category == FileCategory::Other) {
        category = indexing::FileMetadataManager::classifyExtension(ext);
    }

    IContentExtractor* extractor = findExtractor(ext, category);
    if (!extractor) {
        result.status = ExtractionStatus::UnsupportedFormat;
        result.errorDescription = "No registered extractor for format/category";
        auto tEnd = std::chrono::steady_clock::now();
        result.durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        return result;
    }

    result.extractorId = extractor->getExtractorId();
    result.extractorVersion = extractor->getExtractorVersion();

    // Check Cache
    ExtractionResult cachedResult;
    if (m_cache.get(canonPath, fileSize, modTime, result.extractorId, result.extractorVersion, cachedResult)) {
        cachedResult.docId = docId; // Preserve current request DocID
        return cachedResult; // Cache HIT!
    }

    // Cache MISS: Invoke extractor
    result = extractor->extract(filePath, m_maxExtractionBytes, cancelToken);
    result.docId = docId;

    // Cache successful or partial results
    if (result.isSuccess() || result.status == ExtractionStatus::MetadataOnly) {
        m_cache.put(result, modTime);
    }

    return result;
}

ExtractionResult ExtractionManager::extractDocument(DocID docId,
                                                   const indexing::DocumentCatalog& catalog,
                                                   const std::atomic<bool>* cancelToken) {
    const FileMetadata* meta = catalog.getMetadata(docId);
    if (!meta) {
        ExtractionResult r;
        r.docId = docId;
        r.status = ExtractionStatus::Failed;
        r.errorDescription = "Invalid DocID: document not found in catalog";
        return r;
    }

    std::string path = catalog.getPath(docId);
    return extract(path, meta->category, docId, cancelToken);
}

} // namespace sse::extraction
