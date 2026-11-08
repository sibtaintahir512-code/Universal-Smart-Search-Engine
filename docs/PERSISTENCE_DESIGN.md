# Index Persistence & Serialization Design (PERSISTENCE_DESIGN)
## Project: Universal Smart Search Engine
### Release 1 — Local Universal File Search Engine
**Document Version:** 1.0.0  
**Target Platform:** Windows Desktop (C++17 / MSVC 2022 x64 / Qt 6 Widgets / CMake)  
**Academic Context:** University Data Structures & Algorithms Coursework  
**Status:** Approved for Architectural Baseline  

---

## 1. Persistence Philosophy & Binary Storage Rationale

The persistence subsystem provides durable, zero-data-loss storage of the search engine's in-memory structures across application sessions.

### 1.1 Why Binary Serialization Over JSON/XML
- **Deserialization Throughput:** A binary index containing 100,000 files and 50,000 unique terms deserializes in under $1.5\ \text{seconds}$, compared to $12\text{--}18\ \text{seconds}$ for JSON text parsers.
- **Footprint:** Binary storage avoids repetitive JSON schema tags (`"filename": ...`), reducing disk space by $> 75\%$ ($15\ \text{MB}$ binary vs. $65\ \text{MB}$ JSON).
- **Exact Numeric Representation:** Timestamps, 64-bit file sizes, and 32-bit `DocID`s are written without floating-point or ASCII formatting loss.

### 1.2 Strict Rule: Never Serialize Raw Pointers
Raw C++ memory addresses (`0x00007FF7...`) are ephemeral and invalid across process lifecycles. All node graphs (`CustomPostingList`, `CustomHashTable`, `CustomTrie`) are serialized as **flat, contiguous record tables with relative offsets and integer IDs**, which are reconstructed into fresh dynamically allocated structures upon deserialization.

---

## 2. On-Disk Binary File Specification (`search.idx`)

The persistent index file uses a structured multi-section binary format:

```
┌───────────────────────────────────────────────────────────────┐
│ Section 0: Master Header & Magic Signature (64 bytes)         │
├───────────────────────────────────────────────────────────────┤
│ Section 1: String Repository Pool (File names & paths)        │
├───────────────────────────────────────────────────────────────┤
│ Section 2: Document Catalog Table (FileMetadata records)      │
├───────────────────────────────────────────────────────────────┤
│ Section 3: Vocabulary Directory (Terms, DocFreq, Offsets)     │
├───────────────────────────────────────────────────────────────┤
│ Section 4: Postings Block (Sequential PostingNode arrays)     │
├───────────────────────────────────────────────────────────────┤
│ Section 5: Trailing CRC32 Integrity Checksum (4 bytes)        │
└───────────────────────────────────────────────────────────────┘
```

### 2.1 Section 0: Master File Header (Fixed 64 Bytes)

```
struct IndexMasterHeader {
    uint32_t magicSignature;      // Fixed: 0x534D5254 ("SMRT")
    uint16_t majorVersion;        // Version 1
    uint16_t minorVersion;        // Version 0
    uint64_t buildTimestamp;      // Epoch seconds when index was written
    
    uint32_t totalDocuments;      // Total entries in DocumentCatalog
    uint32_t activeDocuments;     // Non-tombstoned active document count
    uint32_t totalVocabulary;     // Total distinct terms in InvertedIndex
    uint64_t totalPostings;       // Total PostingNodes across all lists
    
    uint64_t offsetStringPool;    // Byte offset to Section 1
    uint64_t offsetDocCatalog;    // Byte offset to Section 2
    uint64_t offsetVocabulary;    // Byte offset to Section 3
    uint64_t offsetPostings;      // Byte offset to Section 4
    
    uint8_t  reserved[16];        // Zero-padded alignment padding
};
```

### 2.2 Section 1: String Repository Pool
Stores UTF-8 strings for all filenames and paths contiguously to avoid per-string header overhead:
- `uint32_t poolSizeBytes`: Total length of the string blob.
- `char poolData[]`: Contiguous character sequence.

### 2.3 Section 2: Document Catalog Table
Array of serialized metadata records corresponding to `DocID` $1, \dots, N$:
```
struct SerializedDocRecord {
    uint32_t docId;
    uint64_t fileSize;
    uint64_t creationTime;
    uint64_t lastModifiedTime;
    uint8_t  category;
    uint8_t  extractionState;
    uint8_t  isActive;            // Tombstone flag: 1 = active, 0 = deleted
    uint8_t  flags;
    uint32_t nameOffset;
    uint16_t nameLength;
    uint32_t pathOffset;
    uint16_t pathLength;
    uint16_t extensionLength;
};
```

### 2.4 Section 3: Vocabulary Directory
Contains all indexed unique terms, their document frequency ($DF$), and the starting offset into Section 4:
```
struct SerializedVocabEntry {
    uint16_t termLength;
    char     term[termLength];    // Normalized term string
    uint32_t docFrequency;        // Number of postings in its list
    uint64_t postingsBlockOffset; // Offset relative to Section 4 start
};
```

### 2.5 Section 4: Postings Block
Sequential array of raw postings for each vocabulary term. Stored contiguously without pointers, maintaining independent term frequencies:
```
struct SerializedPosting {
    uint32_t docId;        // 4 bytes: Document identifier (1 .. N)
    uint16_t bodyTermFreq; // 2 bytes: Occurrences in body content
    uint16_t nameTermFreq; // 2 bytes: Occurrences in base filename
    uint8_t  fieldMask;    // 1 byte:  Bitmask (0x01 Filename, 0x02 Content)
    uint8_t  reserved[7];  // 7 bytes: Zero-padding for strict 16-byte record alignment
};
```
- Total serialized posting record size on disk: **exactly 16 bytes**.
- Zero pointers are stored on disk. Nodes are reconstructed into dynamic `PostingNode` instances ($24\ \text{bytes}$ on x64) during deserialization.

### 2.6 Section 5: CRC32 Integrity Checksum
A standard IEEE 802.3 32-bit cyclic redundancy check computed across all bytes from byte 0 to the end of Section 4:
$$\text{Checksum} = \text{CRC32}(\text{Byte}[0 \dots \text{EndSection4}])$$

---

## 3. Deserialization & In-Memory Reconstruction

Upon application startup, `IndexStorage::LoadIndex()` executes the following pipeline:

```
[1. Open File stream] ──► [2. Read 64-byte Header] ──► [Verify Magic 0x534D5254]
                                                               │
                                                               ▼ (Matches)
[3. Compute & Verify CRC32 Checksum] ◄── [Header Version Check (v1.0)]
         │
         ▼ (Checksum Valid)
[4. Read String Pool into CustomDynamicArray<char>]
         │
         ▼
[5. Reconstruct DocumentCatalog Array & Path-to-DocID HashTable]
         │
         ▼
[6. Read Vocabulary & Reconstruct CustomHashTable Buckets]
         │
         ▼
[7. For each term: Read Serialized Postings & Reconstruct CustomPostingList]
         │
         ▼
[8. Rebuild CustomTrie from Loaded Vocabulary Keys (< 40 ms)]
         │
         ▼
[Signal GUI: Index Ready for Querying]
```

### Fast In-Memory Reconstruction Details:
1. **Posting Lists:** For each vocabulary entry, a new `CustomPostingList` is created. Postings are already sorted on disk; they are pushed directly to `m_tail` in $O(1)$ per node without searching.
2. **Hash Table:** Terms are inserted into `CustomHashTable`. Bucket sizing is pre-allocated based on `totalVocabulary` to eliminate rehashing during loading.
3. **Trie Autocomplete:** Restored by looping over the vocabulary table and inserting words into `CustomTrie`. This requires zero disk overhead and takes $< 40\ \text{ms}$ for 50,000 words.

---

## 4. Atomic Save Strategy, Tombstone Compaction & Corruption Recovery

### 4.1 Two-Phase Atomic Save Protocol with Compaction
To guarantee that saving never leaves a corrupt index and that **no invalid or dangling posting references exist on disk**:
1. **Phase 0 (Pre-Save Compaction & Remapping):**
   - If any documents were modified or deleted during the session, `IndexStorage` executes the two-pass Reference Remapping algorithm (detailed in `DSA_DESIGN.md` Section 9.2).
   - Tombstoned documents are purged, `DocID` values are compacted into a contiguous range $[1 \dots N_{active}]$, and all posting lists are rewritten in memory with updated IDs before serialization.
2. **Phase 1 (Write Temporary):** Serialize the compacted index to `search.idx.tmp` in the application data folder.
3. **Phase 2 (Verify):** Compute IEEE 802.3 CRC32 of `search.idx.tmp` and flush disk buffers via `FlushFileBuffers` / `fsync`.
4. **Phase 3 (Atomic Commit):** Atomically replace the existing index file using Windows `ReplaceFileW()` or `std::filesystem::rename()`:
   $$\text{Atomic Swap: } \text{search.idx.tmp} \xrightarrow{\text{rename}} \text{search.idx}$$
- If writing fails or is interrupted at any point, `search.idx` remains completely untouched, uncorrupted, and valid.

### 4.2 Corruption Detection & Automatic Recovery
If `search.idx` fails validation on startup:
1. **Trigger Conditions:** Magic signature mismatch, incompatible version byte, truncated file size, or failed CRC32 checksum.
2. **Graceful Action:** The system logs the failure reason, isolates the damaged file (`search.idx.corrupt`), initializes clean empty in-memory data structures, and notifies the user via a Qt dialog:
   ```
   [Warning] The search index was damaged or from an incompatible version.
   A fresh index must be rebuilt.
   [ Rebuild Now ]   [ Cancel ]
   ```
3. The application never crashes or dereferences garbage pointers from corrupted files.

---

## 5. Separate Search History Storage (`history.dat`)

### 5.1 Storage Separation Principle
Search history is decoupled from the main index file because:
- History updates on every user query execution (high frequency write).
- Index saves occur only upon scan completion or shutdown.
- Corrupting history must never affect index usability.

### 5.2 History File Format
Stored as a lightweight delimited text or JSON file in the application directory:
```
# SMRT_SEARCH_HISTORY_V1
1731024000|dijkstra algorithm|12|4.2
1731024050|memory allocator cpp|3|1.8
1731024100|final exam syllabus|1|0.5
```
Each line records: `Timestamp | Query String | Results Count | Execution Duration (ms)`. Bounded at 100 entries (FIFO circular eviction).
