#include "persistence/IndexDeserializer.hpp"
#include <chrono>

namespace sse::persistence {

PersistenceResult IndexDeserializer::validate(const uint8_t* data, std::size_t size) {
    if (!data || size < (INDEX_HEADER_SIZE + sizeof(uint32_t))) {
        return PersistenceResult::failure(PersistenceStatus::CorruptedHeader, "Index file buffer too small or null");
    }

    if (size > MAX_INDEX_FILE_SIZE) {
        return PersistenceResult::failure(PersistenceStatus::FileSizeLimitExceeded, "Index file exceeds maximum allowed size (2GB)");
    }

    // 1. Verify Trailing File Checksum
    uint32_t computedFileCrc = CRC32::calculate(data, size - sizeof(uint32_t));
    uint32_t expectedFileCrc = static_cast<uint32_t>(data[size - 4]) |
                              (static_cast<uint32_t>(data[size - 3]) << 8) |
                              (static_cast<uint32_t>(data[size - 2]) << 16) |
                              (static_cast<uint32_t>(data[size - 1]) << 24);

    if (computedFileCrc != expectedFileCrc) {
        return PersistenceResult::failure(PersistenceStatus::ChecksumMismatch, "Trailing file CRC32 checksum mismatch");
    }

    // 2. Validate Master Header
    BinaryReader reader(data, size);
    IndexFileHeader header;
    if (!header.deserialize(reader)) {
        return PersistenceResult::failure(PersistenceStatus::CorruptedHeader, "Invalid master header (signature, version, or header CRC32 mismatch)");
    }

    if (header.totalFileSize != size) {
        return PersistenceResult::failure(PersistenceStatus::CorruptedHeader, "Header totalFileSize does not match actual byte size");
    }

    if (header.sectionCount > MAX_SECTION_COUNT || header.sectionCount == 0) {
        return PersistenceResult::failure(PersistenceStatus::ResourceLimitExceeded, "Invalid section count in header");
    }

    if (header.sectionDirectoryOffset + header.sectionCount * 48 > size - sizeof(uint32_t)) {
        return PersistenceResult::failure(PersistenceStatus::CorruptedSection, "Section directory offset out of bounds");
    }

    // 3. Validate Section Descriptors and Section CRC32s
    if (!reader.seek(header.sectionDirectoryOffset)) {
        return PersistenceResult::failure(PersistenceStatus::CorruptedSection, "Failed to seek to section directory");
    }

    dsa::CustomDynamicArray<IndexSectionDescriptor> descriptors;
    for (uint32_t i = 0; i < header.sectionCount; ++i) {
        IndexSectionDescriptor desc;
        if (!desc.deserialize(reader)) {
            return PersistenceResult::failure(PersistenceStatus::CorruptedSection, "Failed to read section descriptor");
        }

        if (desc.offset + desc.storedSize > size - sizeof(uint32_t)) {
            return PersistenceResult::failure(PersistenceStatus::CorruptedSection, "Section offset + size exceeds file boundary");
        }

        if (desc.storedSize > MAX_SECTION_SIZE) {
            return PersistenceResult::failure(PersistenceStatus::ResourceLimitExceeded, "Section size exceeds maximum allowed limit");
        }

        uint32_t secCrc = CRC32::calculate(data + desc.offset, desc.storedSize);
        if (secCrc != desc.checksum) {
            return PersistenceResult::failure(PersistenceStatus::ChecksumMismatch, "Section CRC32 checksum mismatch");
        }

        descriptors.push_back(desc);
    }

    PersistenceResult res = PersistenceResult::success("Index validation passed");
    res.documentCount = header.totalDocuments;
    res.activeDocumentCount = header.activeDocuments;
    res.vocabularySize = header.totalVocabulary;
    res.bytesProcessed = size;
    res.generation = header.generation;
    return res;
}

PersistenceResult IndexDeserializer::deserialize(
    const uint8_t* data,
    std::size_t size,
    indexing::DocumentCatalog& outCatalog,
    indexing::InvertedIndex& outIndex,
    const std::atomic<bool>* cancelToken) {

    auto tStart = std::chrono::high_resolution_clock::now();

    if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
        return PersistenceResult::failure(PersistenceStatus::Cancelled, "Deserialization cancelled prior to start");
    }

    // 1. Dry-run structural validation
    PersistenceResult valRes = validate(data, size);
    if (!valRes.isSuccess()) {
        return valRes;
    }

    BinaryReader headerReader(data, size);
    IndexFileHeader header;
    header.deserialize(headerReader);

    headerReader.seek(header.sectionDirectoryOffset);
    dsa::CustomDynamicArray<IndexSectionDescriptor> descriptors;
    for (uint32_t i = 0; i < header.sectionCount; ++i) {
        IndexSectionDescriptor desc;
        desc.deserialize(headerReader);
        descriptors.push_back(desc);
    }

    // Staging structures to guarantee transactionality: live index untouched until complete success
    indexing::DocumentCatalog stagedCatalog;
    indexing::InvertedIndex stagedIndex;

    const IndexSectionDescriptor* descTokenizer = nullptr;
    const IndexSectionDescriptor* descCatalog = nullptr;
    const IndexSectionDescriptor* descLengths = nullptr;
    const IndexSectionDescriptor* descVocab = nullptr;
    const IndexSectionDescriptor* descPostings = nullptr;

    for (std::size_t i = 0; i < descriptors.size(); ++i) {
        switch (descriptors[i].sectionType) {
            case SectionType::TokenizerInfo:        descTokenizer = &descriptors[i]; break;
            case SectionType::DocumentCatalog:      descCatalog = &descriptors[i]; break;
            case SectionType::DocumentLengths:      descLengths = &descriptors[i]; break;
            case SectionType::VocabularyDictionary: descVocab = &descriptors[i]; break;
            case SectionType::PostingsBlock:        descPostings = &descriptors[i]; break;
            default: break;
        }
    }

    if (!descCatalog || !descVocab || !descPostings) {
        return PersistenceResult::failure(PersistenceStatus::CorruptedSection, "Missing mandatory index sections");
    }

    // 2. Parse Tokenizer Info
    if (descTokenizer) {
        BinaryReader rTok(data + descTokenizer->offset, descTokenizer->storedSize);
        uint16_t tokVer = 0, normVer = 0;
        if (!rTok.readUint16(tokVer) || !rTok.readUint16(normVer)) {
            return PersistenceResult::failure(PersistenceStatus::CorruptedSection, "Failed reading tokenizer section");
        }
        if (tokVer != 1 || normVer != 1) {
            return PersistenceResult::failure(PersistenceStatus::IncompatibleTokenizer, "Incompatible tokenizer version; rebuild required");
        }
    }

    // 3. Parse Document Catalog
    {
        BinaryReader rCat(data + descCatalog->offset, descCatalog->storedSize);
        uint32_t totalDocs = 0;
        if (!rCat.readUint32(totalDocs)) {
            return PersistenceResult::failure(PersistenceStatus::CorruptedSection, "Failed reading catalog document count");
        }

        if (totalDocs > MAX_DOCUMENT_COUNT) {
            return PersistenceResult::failure(PersistenceStatus::ResourceLimitExceeded, "Catalog total documents exceeds limit");
        }

        for (uint32_t i = 0; i < totalDocs; ++i) {
            if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
                return PersistenceResult::failure(PersistenceStatus::Cancelled, "Deserialization cancelled during catalog decoding");
            }

            FileMetadata meta{};
            uint8_t cat = 0, extState = 0, isActive = 0;
            std::string path, filename;

            if (!rCat.readUint32(meta.docId) ||
                !rCat.readUint64(meta.fileSize) ||
                !rCat.readUint64(meta.creationTime) ||
                !rCat.readUint64(meta.lastModifiedTime) ||
                !rCat.readUint8(cat) ||
                !rCat.readUint8(extState) ||
                !rCat.readUint8(isActive) ||
                !rCat.readUint8(meta.flags) ||
                !rCat.readString16(path, MAX_STRING_LENGTH) ||
                !rCat.readString16(filename, MAX_STRING_LENGTH)) {
                return PersistenceResult::failure(PersistenceStatus::CorruptedSection, "Failed reading document catalog record");
            }

            meta.category = static_cast<FileCategory>(cat);
            meta.extractionState = static_cast<ExtractionStatus>(extState);

            stagedCatalog.restoreDocument(meta, path, filename, isActive != 0);
        }
    }

    // 4. Parse Vocabulary and Postings
    uint64_t totalPostingsRestored = 0;
    {
        BinaryReader rVocab(data + descVocab->offset, descVocab->storedSize);
        BinaryReader rPostings(data + descPostings->offset, descPostings->storedSize);

        uint32_t vocabCount = 0;
        if (!rVocab.readUint32(vocabCount)) {
            return PersistenceResult::failure(PersistenceStatus::CorruptedSection, "Failed reading vocabulary count");
        }

        if (vocabCount > MAX_VOCABULARY_COUNT) {
            return PersistenceResult::failure(PersistenceStatus::ResourceLimitExceeded, "Vocabulary count exceeds limit");
        }

        for (uint32_t i = 0; i < vocabCount; ++i) {
            if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
                return PersistenceResult::failure(PersistenceStatus::Cancelled, "Deserialization cancelled during vocabulary decoding");
            }

            std::string term;
            uint32_t df = 0;
            uint64_t postingsOffset = 0;

            if (!rVocab.readString16(term, MAX_STRING_LENGTH) ||
                !rVocab.readUint32(df) ||
                !rVocab.readUint64(postingsOffset)) {
                return PersistenceResult::failure(PersistenceStatus::CorruptedSection, "Failed reading vocabulary term entry");
            }

            if (postingsOffset + static_cast<uint64_t>(df) * 16 > descPostings->storedSize) {
                return PersistenceResult::failure(PersistenceStatus::CorruptedSection, "Postings offset out of bounds in postings section");
            }

            if (!rPostings.seek(postingsOffset)) {
                return PersistenceResult::failure(PersistenceStatus::CorruptedSection, "Failed seeking to postings offset");
            }

            for (uint32_t p = 0; p < df; ++p) {
                DocID docId = 0;
                uint16_t nameFreq = 0, bodyFreq = 0;
                uint8_t fieldMask = 0;
                uint8_t padding[7];

                if (!rPostings.readUint32(docId) ||
                    !rPostings.readUint16(nameFreq) ||
                    !rPostings.readUint16(bodyFreq) ||
                    !rPostings.readUint8(fieldMask) ||
                    !rPostings.readBytes(padding, 7)) {
                    return PersistenceResult::failure(PersistenceStatus::CorruptedSection, "Failed reading posting node");
                }

                if (docId == INVALID_DOC_ID || docId > header.totalDocuments) {
                    return PersistenceResult::failure(PersistenceStatus::CorruptedSection, "Invalid DocID in posting node");
                }

                stagedIndex.restorePosting(term, docId, nameFreq, bodyFreq, fieldMask);
                ++totalPostingsRestored;
            }
        }
    }

    // 5. Parse Document Length Statistics
    if (descLengths) {
        BinaryReader rLengths(data + descLengths->offset, descLengths->storedSize);
        uint64_t totName = 0, totBody = 0;
        uint32_t actDocs = 0, docCnt = 0;

        if (!rLengths.readUint64(totName) ||
            !rLengths.readUint64(totBody) ||
            !rLengths.readUint32(actDocs) ||
            !rLengths.readUint32(docCnt)) {
            return PersistenceResult::failure(PersistenceStatus::CorruptedSection, "Failed reading length statistics header");
        }

        for (uint32_t i = 0; i < docCnt; ++i) {
            DocID docId = 0;
            uint32_t nameLen = 0, bodyLen = 0;
            uint8_t isAct = 0;

            if (!rLengths.readUint32(docId) ||
                !rLengths.readUint32(nameLen) ||
                !rLengths.readUint32(bodyLen) ||
                !rLengths.readUint8(isAct)) {
                return PersistenceResult::failure(PersistenceStatus::CorruptedSection, "Failed reading document length record");
            }

            if (isAct) {
                stagedIndex.documentLengthStatistics().recordDocumentLength(docId, nameLen, bodyLen);
            }
        }
    }

    if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
        return PersistenceResult::failure(PersistenceStatus::Cancelled, "Deserialization cancelled before commit");
    }

    // 6. Transactional Commit: Atomically publish restored state
    outCatalog = std::move(stagedCatalog);
    outIndex = std::move(stagedIndex);

    auto tEnd = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    PersistenceResult result = PersistenceResult::success("Index restored successfully");
    result.documentCount = header.totalDocuments;
    result.activeDocumentCount = header.activeDocuments;
    result.vocabularySize = header.totalVocabulary;
    result.postingsCount = totalPostingsRestored;
    result.bytesProcessed = size;
    result.durationMs = durationMs;
    result.generation = header.generation;
    return result;
}

} // namespace sse::persistence
