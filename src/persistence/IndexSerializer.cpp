#include "persistence/IndexSerializer.hpp"
#include <chrono>

namespace sse::persistence {

PersistenceResult IndexSerializer::serialize(
    const indexing::DocumentCatalog& catalog,
    const indexing::InvertedIndex& index,
    BinaryWriter& outWriter,
    uint64_t generation,
    const std::atomic<bool>* cancelToken) {

    auto tStart = std::chrono::high_resolution_clock::now();

    if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
        return PersistenceResult::failure(PersistenceStatus::Cancelled, "Serialization cancelled prior to start");
    }

    // 1. Serialize Section 1: Tokenizer Info
    BinaryWriter writerTokenizer(64);
    writerTokenizer.writeUint16(1); // tokenizerVersion
    writerTokenizer.writeUint16(1); // normalizerVersion
    writerTokenizer.writeUint32(1); // minTokenLength
    writerTokenizer.writeUint32(128); // maxTokenLength
    writerTokenizer.writeUint8(1); // caseFolding: lowercase
    writerTokenizer.writeUint8(1); // stripPunctuation: true
    writerTokenizer.writeUint8(0); // reserved
    writerTokenizer.writeUint8(0); // reserved

    // 2. Serialize Section 2: Document Catalog
    uint32_t totalDocs = static_cast<uint32_t>(catalog.totalCount());
    uint32_t activeDocs = static_cast<uint32_t>(catalog.activeCount());

    BinaryWriter writerCatalog(totalDocs * 64 + 32);
    writerCatalog.writeUint32(totalDocs);

    for (DocID id = 1; id <= totalDocs; ++id) {
        if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
            return PersistenceResult::failure(PersistenceStatus::Cancelled, "Serialization cancelled during catalog encoding");
        }

        const FileMetadata* meta = catalog.getMetadata(id);
        if (meta) {
            writerCatalog.writeUint32(id);
            writerCatalog.writeUint64(meta->fileSize);
            writerCatalog.writeUint64(meta->creationTime);
            writerCatalog.writeUint64(meta->lastModifiedTime);
            writerCatalog.writeUint8(static_cast<uint8_t>(meta->category));
            writerCatalog.writeUint8(static_cast<uint8_t>(meta->extractionState));
            writerCatalog.writeUint8(catalog.isActive(id) ? 1 : 0);
            writerCatalog.writeUint8(meta->flags);
            writerCatalog.writeString16(catalog.getPath(id));
            writerCatalog.writeString16(catalog.getFilename(id));
        } else {
            writerCatalog.writeUint32(id);
            writerCatalog.writeUint64(0);
            writerCatalog.writeUint64(0);
            writerCatalog.writeUint64(0);
            writerCatalog.writeUint8(0);
            writerCatalog.writeUint8(0);
            writerCatalog.writeUint8(0);
            writerCatalog.writeUint8(0);
            writerCatalog.writeString16("");
            writerCatalog.writeString16("");
        }
    }

    // 3. Serialize Section 3: Document Length Statistics
    const auto& lengthStats = index.documentLengthStatistics();
    BinaryWriter writerLengths(totalDocs * 16 + 32);
    writerLengths.writeUint64(lengthStats.totalNameTokens());
    writerLengths.writeUint64(lengthStats.totalBodyTokens());
    writerLengths.writeUint32(static_cast<uint32_t>(lengthStats.activeDocumentCount()));
    writerLengths.writeUint32(totalDocs);

    for (DocID id = 1; id <= totalDocs; ++id) {
        writerLengths.writeUint32(id);
        writerLengths.writeUint32(lengthStats.getNameLength(id));
        writerLengths.writeUint32(lengthStats.getBodyLength(id));
        writerLengths.writeUint8(lengthStats.hasDocument(id) ? 1 : 0);
    }

    // 4. Serialize Section 4 & 5: Vocabulary Dictionary and Postings Block
    uint32_t vocabCount = static_cast<uint32_t>(index.vocabularySize());
    BinaryWriter writerVocab(vocabCount * 32 + 32);
    BinaryWriter writerPostings(static_cast<std::size_t>(index.totalPostingsCount() * 16 + 32));

    writerVocab.writeUint32(vocabCount);

    uint64_t currentPostingsOffset = 0;
    uint64_t totalPostingsWritten = 0;

    index.forEachTerm([&](const std::string& term, const dsa::CustomPostingList& list) {
        writerVocab.writeString16(term);
        uint32_t df = list.documentFrequency();
        writerVocab.writeUint32(df);
        writerVocab.writeUint64(currentPostingsOffset);

        const PostingNode* node = list.head();
        while (node) {
            writerPostings.writeUint32(node->docId);
            writerPostings.writeUint16(node->nameTermFreq);
            writerPostings.writeUint16(node->bodyTermFreq);
            writerPostings.writeUint8(node->fieldMask);
            for (int p = 0; p < 7; ++p) {
                writerPostings.writeUint8(0); // 7-byte padding for 16-byte alignment
            }

            currentPostingsOffset += 16;
            ++totalPostingsWritten;
            node = node->next;
        }
    });

    if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
        return PersistenceResult::failure(PersistenceStatus::Cancelled, "Serialization cancelled after index encoding");
    }

    // 5. Construct Section Descriptors
    constexpr uint32_t SECTION_COUNT = 5;
    constexpr uint64_t DIRECTORY_SIZE = static_cast<uint64_t>(SECTION_COUNT) * 48;
    uint64_t currentOffset = INDEX_HEADER_SIZE + DIRECTORY_SIZE;

    IndexSectionDescriptor descTokenizer;
    descTokenizer.sectionType = SectionType::TokenizerInfo;
    descTokenizer.sectionVersion = 1;
    descTokenizer.flags = SECTION_FLAG_REQUIRED;
    descTokenizer.offset = currentOffset;
    descTokenizer.storedSize = writerTokenizer.size();
    descTokenizer.uncompressedSize = writerTokenizer.size();
    descTokenizer.entryCount = 1;
    descTokenizer.checksum = CRC32::calculate(writerTokenizer.data(), writerTokenizer.size());
    currentOffset += descTokenizer.storedSize;

    IndexSectionDescriptor descCatalog;
    descCatalog.sectionType = SectionType::DocumentCatalog;
    descCatalog.sectionVersion = 1;
    descCatalog.flags = SECTION_FLAG_REQUIRED;
    descCatalog.offset = currentOffset;
    descCatalog.storedSize = writerCatalog.size();
    descCatalog.uncompressedSize = writerCatalog.size();
    descCatalog.entryCount = totalDocs;
    descCatalog.checksum = CRC32::calculate(writerCatalog.data(), writerCatalog.size());
    currentOffset += descCatalog.storedSize;

    IndexSectionDescriptor descLengths;
    descLengths.sectionType = SectionType::DocumentLengths;
    descLengths.sectionVersion = 1;
    descLengths.flags = SECTION_FLAG_REQUIRED;
    descLengths.offset = currentOffset;
    descLengths.storedSize = writerLengths.size();
    descLengths.uncompressedSize = writerLengths.size();
    descLengths.entryCount = totalDocs;
    descLengths.checksum = CRC32::calculate(writerLengths.data(), writerLengths.size());
    currentOffset += descLengths.storedSize;

    IndexSectionDescriptor descVocab;
    descVocab.sectionType = SectionType::VocabularyDictionary;
    descVocab.sectionVersion = 1;
    descVocab.flags = SECTION_FLAG_REQUIRED;
    descVocab.offset = currentOffset;
    descVocab.storedSize = writerVocab.size();
    descVocab.uncompressedSize = writerVocab.size();
    descVocab.entryCount = vocabCount;
    descVocab.checksum = CRC32::calculate(writerVocab.data(), writerVocab.size());
    currentOffset += descVocab.storedSize;

    IndexSectionDescriptor descPostings;
    descPostings.sectionType = SectionType::PostingsBlock;
    descPostings.sectionVersion = 1;
    descPostings.flags = SECTION_FLAG_REQUIRED;
    descPostings.offset = currentOffset;
    descPostings.storedSize = writerPostings.size();
    descPostings.uncompressedSize = writerPostings.size();
    descPostings.entryCount = totalPostingsWritten;
    descPostings.checksum = CRC32::calculate(writerPostings.data(), writerPostings.size());
    currentOffset += descPostings.storedSize;

    uint64_t totalFileSize = currentOffset + sizeof(uint32_t); // includes 4-byte trailing checksum

    // 6. Build Master Header
    IndexFileHeader header;
    header.magicSignature = INDEX_MAGIC_SIGNATURE;
    header.majorVersion = INDEX_MAJOR_VERSION;
    header.minorVersion = INDEX_MINOR_VERSION;
    header.headerSize = INDEX_HEADER_SIZE;
    header.endiannessMarker = INDEX_ENDIANNESS_MARKER;
    header.featureFlags = 0x01; // Has document length statistics
    header.sectionCount = SECTION_COUNT;
    header.sectionDirectoryOffset = INDEX_HEADER_SIZE;
    header.generation = generation;
    header.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    header.totalFileSize = totalFileSize;
    header.totalDocuments = totalDocs;
    header.activeDocuments = activeDocs;
    header.totalVocabulary = vocabCount;
    header.totalPostings = static_cast<uint32_t>(totalPostingsWritten);

    // 7. Assemble Full Binary Stream
    outWriter.clear();
    header.serialize(outWriter);

    descTokenizer.serialize(outWriter);
    descCatalog.serialize(outWriter);
    descLengths.serialize(outWriter);
    descVocab.serialize(outWriter);
    descPostings.serialize(outWriter);

    outWriter.writeBytes(writerTokenizer.data(), writerTokenizer.size());
    outWriter.writeBytes(writerCatalog.data(), writerCatalog.size());
    outWriter.writeBytes(writerLengths.data(), writerLengths.size());
    outWriter.writeBytes(writerVocab.data(), writerVocab.size());
    outWriter.writeBytes(writerPostings.data(), writerPostings.size());

    // 8. Compute and Append Trailing Whole-File CRC32 Checksum
    uint32_t fileChecksum = CRC32::calculate(outWriter.data(), outWriter.size());
    outWriter.writeUint32(fileChecksum);

    auto tEnd = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    PersistenceResult result = PersistenceResult::success("Index serialized successfully");
    result.bytesProcessed = outWriter.size();
    result.documentCount = totalDocs;
    result.activeDocumentCount = activeDocs;
    result.vocabularySize = vocabCount;
    result.postingsCount = totalPostingsWritten;
    result.durationMs = durationMs;
    result.generation = generation;
    return result;
}

} // namespace sse::persistence
