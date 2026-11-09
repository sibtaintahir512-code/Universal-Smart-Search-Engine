#include "indexing/IndexingPipeline.hpp"
#include <chrono>
#include "dsa/CustomHashTable.hpp"

namespace sse::indexing {

namespace {

struct LocalTermFreq {
    uint16_t nameFreq{0};
    uint16_t bodyFreq{0};
    uint8_t  fieldMask{0};
};

} // anonymous namespace

IndexingPipeline::IndexingPipeline(DocumentCatalog& catalog,
                                   InvertedIndex& index,
                                   Tokenizer tokenizer,
                                   extraction::ExtractionManager* extractionManager)
    : m_catalog(catalog),
      m_index(index),
      m_tokenizer(std::move(tokenizer)),
      m_extractionManager(extractionManager) {}

IndexingResult IndexingPipeline::indexDocument(DocID docId, const extraction::ExtractionResult* optExtraction) {
    auto tStart = std::chrono::steady_clock::now();
    IndexingResult result;
    result.docId = docId;

    if (docId == INVALID_DOC_ID || !m_catalog.isActive(docId)) {
        result.success = false;
        result.message = "Invalid or inactive DocID";
        ++m_stats.documentsFailed;
        return result;
    }

    const FileMetadata* meta = m_catalog.getMetadata(docId);
    if (!meta) {
        result.success = false;
        result.message = "DocID metadata not found in catalog";
        ++m_stats.documentsFailed;
        return result;
    }

    std::string path = m_catalog.getPath(docId);
    std::string filename = m_catalog.getFilename(docId);
    result.filePath = path;

    if (filename.empty()) {
        result.success = false;
        result.message = "Document filename is empty";
        ++m_stats.documentsFailed;
        return result;
    }

    // Determine extraction result
    extraction::ExtractionResult localExtraction;
    const extraction::ExtractionResult* extractionPtr = optExtraction;

    if (!extractionPtr) {
        if (m_extractionManager) {
            localExtraction = m_extractionManager->extract(path, meta->category, docId);
            extractionPtr = &localExtraction;
        } else {
            localExtraction.docId = docId;
            localExtraction.sourcePath = path;
            localExtraction.status = extraction::ExtractionStatus::MetadataOnly;
            extractionPtr = &localExtraction;
        }
    }

    // Check extraction status per specification
    bool indexBody = false;
    bool isPartial = false;
    bool filenameOnly = false;

    switch (extractionPtr->status) {
        case extraction::ExtractionStatus::Success:
            indexBody = true;
            isPartial = false;
            filenameOnly = false;
            break;
        case extraction::ExtractionStatus::Partial:
            indexBody = true;
            isPartial = true;
            filenameOnly = false;
            break;
        case extraction::ExtractionStatus::Cancelled:
            result.success = false;
            result.message = "Extraction was cancelled by user or scanner";
            ++m_stats.documentsFailed;
            return result;
        case extraction::ExtractionStatus::MetadataOnly:
        case extraction::ExtractionStatus::UnsupportedFormat:
        case extraction::ExtractionStatus::Corrupted:
        case extraction::ExtractionStatus::Encrypted:
        case extraction::ExtractionStatus::FileTooLarge:
        case extraction::ExtractionStatus::PermissionDenied:
        case extraction::ExtractionStatus::Failed:
        default:
            indexBody = false;
            isPartial = false;
            filenameOnly = true;
            break;
    }

    // Step 1: Tokenize Filename
    auto nameTokens = m_tokenizer.tokenize(filename, FIELD_FILENAME);

    // Step 2: Tokenize Body (if available and not restricted to metadata-only)
    dsa::CustomDynamicArray<Token> bodyTokens;
    if (indexBody && extractionPtr->hasText()) {
        bodyTokens = m_tokenizer.tokenize(extractionPtr->extractedText, FIELD_CONTENT);
    }

    // Step 3: Accumulate local per-document term frequencies
    dsa::CustomHashTable<std::string, LocalTermFreq> localFrequencies;
    uint32_t tokenCount = 0;
    uint32_t nameTokenCount = 0;
    uint32_t bodyTokenCount = 0;

    for (std::size_t i = 0; i < nameTokens.size(); ++i) {
        const Token& tok = nameTokens[i];
        LocalTermFreq* entry = localFrequencies.find(tok.term);
        if (!entry) {
            LocalTermFreq init;
            init.nameFreq = 1;
            init.fieldMask = FIELD_FILENAME;
            localFrequencies.insert(tok.term, init);
        } else {
            if (entry->nameFreq < UINT16_MAX) ++entry->nameFreq;
            entry->fieldMask |= FIELD_FILENAME;
        }
        ++tokenCount;
        ++nameTokenCount;
    }

    for (std::size_t i = 0; i < bodyTokens.size(); ++i) {
        const Token& tok = bodyTokens[i];
        LocalTermFreq* entry = localFrequencies.find(tok.term);
        if (!entry) {
            LocalTermFreq init;
            init.bodyFreq = 1;
            init.fieldMask = FIELD_CONTENT;
            localFrequencies.insert(tok.term, init);
        } else {
            if (entry->bodyFreq < UINT16_MAX) ++entry->bodyFreq;
            entry->fieldMask |= FIELD_CONTENT;
        }
        ++tokenCount;
        ++bodyTokenCount;
    }

    // Step 4: Atomic commit into the primary InvertedIndex
    std::size_t uniqueTerms = localFrequencies.size();
    auto** buckets = localFrequencies.getBuckets();
    std::size_t numBuckets = localFrequencies.bucketCount();

    for (std::size_t b = 0; b < numBuckets; ++b) {
        auto* entry = buckets[b];
        while (entry) {
            m_index.addTerm(entry->key,
                            docId,
                            entry->value.nameFreq,
                            entry->value.bodyFreq,
                            entry->value.fieldMask);
            entry = entry->next;
        }
    }

    // Set precise token counts in DocumentLengthStatistics
    m_index.documentLengthStatistics().recordDocumentLength(docId, nameTokenCount, bodyTokenCount);

    // Step 5: Update Catalog Extraction State
    FileMetadata* mutMeta = m_catalog.getMutableMetadata(docId);
    if (mutMeta) {
        if (filenameOnly) {
            mutMeta->extractionState = sse::ExtractionStatus::MetadataOnly;
        } else if (isPartial) {
            mutMeta->extractionState = sse::ExtractionStatus::Truncated;
        } else {
            mutMeta->extractionState = sse::ExtractionStatus::Extracted;
        }
    }

    // Step 6: Update Statistics
    auto tEnd = std::chrono::steady_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    ++m_stats.documentsProcessed;
    if (filenameOnly) {
        ++m_stats.documentsFilenameOnly;
    } else if (isPartial) {
        ++m_stats.documentsPartiallyIndexed;
    } else {
        ++m_stats.documentsFullyIndexed;
    }

    m_stats.totalTokensProcessed += tokenCount;
    m_stats.totalFilenameTermOccurrences += nameTokenCount;
    m_stats.totalBodyTermOccurrences += bodyTokenCount;
    m_stats.uniqueVocabularyCount = m_index.vocabularySize();
    m_stats.totalPostingsCount = m_index.totalPostingsCount();
    m_stats.indexingDurationMs += durationMs;

    result.success = true;
    result.isPartial = isPartial;
    result.filenameOnly = filenameOnly;
    result.tokensIndexed = tokenCount;
    result.uniqueTermsIndexed = static_cast<uint32_t>(uniqueTerms);
    result.durationMs = durationMs;
    result.message = filenameOnly ? "Indexed filename only" : (isPartial ? "Indexed partially" : "Indexed fully");

    return result;
}

IndexingResult IndexingPipeline::updateDocument(DocID docId, const extraction::ExtractionResult* optExtraction) {
    // Atomically purge previous postings for this DocID to prevent stale terms or duplicates
    m_index.removeDocument(docId);
    return indexDocument(docId, optExtraction);
}

bool IndexingPipeline::removeDocument(DocID docId) {
    bool removed = m_index.removeDocument(docId);
    m_catalog.markDeleted(docId);
    m_stats.uniqueVocabularyCount = m_index.vocabularySize();
    m_stats.totalPostingsCount = m_index.totalPostingsCount();
    return removed;
}

} // namespace sse::indexing
