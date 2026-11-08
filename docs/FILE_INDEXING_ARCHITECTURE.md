# Universal File Indexing Subsystem Architecture (FILE_INDEXING_ARCHITECTURE)
## Project: Universal Smart Search Engine
### Release 1 — Local Universal File Search Engine
**Document Version:** 1.0.0  
**Target Platform:** Windows Desktop (C++17 / MSVC 2022 x64 / Qt 6 Widgets / CMake)  
**Academic Context:** University Data Structures & Algorithms Coursework  
**Status:** Approved for Architectural Baseline  

---

## 1. Subsystem Overview & Core Architecture

The Universal File Indexing Subsystem is responsible for discovering, classifying, cataloging, and indexing files across arbitrary user-specified local directory hierarchies and physical storage drives. 

To resolve the fundamental tension between indexing speed, memory consumption, and file format diversity, the subsystem divides indexing into two distinct, decoupled pipelines that operate in concert:

```
                               ┌────────────────────────────────┐
                               │  Filesystem Discovery Stream   │
                               │  (FileScanner BFS CustomQueue) │
                               └───────────────┬────────────────┘
                                               │
                                               ▼
                               ┌────────────────────────────────┐
                               │   Canonical Path & Duplicate   │
                               │      Detection Filter          │
                               └───────────────┬────────────────┘
                                               │
                                               ▼
                               ┌────────────────────────────────┐
                               │    Metadata Extraction Engine  │
                               │     (FileMetadataManager)      │
                               └───────────────┬────────────────┘
                                               │
                                               ▼
                               ┌────────────────────────────────┐
                               │     DocumentCatalog Ingestion  │
                               │     - Assign Dense 32-bit DocID│
                               │     - Store in DynamicArray    │
                               │     - Index Filename Tokens    │
                               └───────────────┬────────────────┘
                                               │
                                               ▼
                              Can Content Be Safely Extracted?
                              (TXT, MD, Code, PDF, DOCX, <= 50MB)
                                               │
                        ┌──────────────────────┴──────────────────────┐
                        │ YES                                         │ NO
                        ▼                                             ▼
         ┌──────────────────────────────┐              ┌──────────────────────────────┐
         │     ContentExtractor         │              │   Metadata-Only Fallback     │
         │ - Format-specific decoders   │              │ - Set Status = MetadataOnly  │
         │ - Stream UTF-8 text buffer   │              │ - Retain in DocumentCatalog  │
         └──────────────┬───────────────┘              │ - Filename tokens searchable │
                        │                              └──────────────┬───────────────┘
            Extraction Successful?                                    │
                        │                                             │
                 ┌──────┴──────┐                                      │
                 │ YES         │ NO                                   │
                 ▼             ▼                                      │
         ┌──────────────┐ ┌──────────────┐                            │
         │ TextProcessor│ │ Status =     │                            │
         │ - Tokenize   │ │ Extraction-  │                            │
         │ - Stop Words │ │ Failed       │                            │
         └──────┬───────┘ └──────┬───────┘                            │
                │                │                                    │
                ▼                │                                    │
         ┌──────────────┐        │                                    │
         │ InvertedIndex│        │                                    │
         │ - Postings   │        │                                    │
         └──────┬───────┘        │                                    │
                │                │                                    │
                └────────────────┴────────────────────────────────────┘
                                 │
                                 ▼
                     Index Persistence Pipeline
```

---

## 2. System A: Metadata Indexing System

### 2.1 Metadata Schema & Record Layout
Every discovered file system object is allocated a `FileMetadata` record stored in the `DocumentCatalog`. Metadata is captured in $O(1)$ time per file using native OS attribute queries (`std::filesystem::directory_entry`).

```
struct FileMetadata {
    uint32_t docId;                  // Unique sequential identifier (1 .. N)
    uint64_t fileSize;               // File size in bytes
    uint64_t creationTime;           // Epoch seconds (fallback to 0 if unsupported)
    uint64_t lastModifiedTime;       // Epoch seconds (used for change detection)
    
    FileCategory category;           // 8-bit enum (Documents, Code, Images, etc.)
    ExtractionStatus extractionState;// 8-bit enum (Pending, Extracted, MetadataOnly, Failed)
    
    uint32_t nameOffset;             // Offset into string repository buffer
    uint16_t nameLength;             // Length of filename string
    uint32_t pathOffset;             // Offset into string repository buffer
    uint16_t pathLength;             // Length of full absolute path
    uint16_t extensionLength;        // Length of extension string (excluding dot)
    
    uint8_t flags;                   // Bit 0: is_active (tombstone flag)
                                     // Bit 1: has_thumbnail
                                     // Bit 2: is_symlink
};
```

### 2.2 Storage & Memory Layout: Compact String Pool
Rather than storing three separate `std::string` objects per file (which would incur substantial heap fragmentation: $3 \times 32\ \text{bytes} = 96\ \text{bytes}$ heap overhead per file), `DocumentCatalog` employs a **Compact Contiguous String Pool** (`CustomDynamicArray<char>`).
- File names and paths are written contiguously into the byte pool.
- `FileMetadata` stores 32-bit offsets and 16-bit lengths.
- Result: Fixed `sizeof(FileMetadata) = 36\ \text{bytes}` per file, enabling 100,000 files to reside in just $3.6\ \text{MB}$ of structured memory plus path strings ($\approx 12\ \text{MB}$ total).

### 2.3 Attributes Indexed
1. **Unique Document ID (`DocID`):** Dense 32-bit integer.
2. **File Name:** Normalized base filename without path (e.g., `Architecture_Report_2026.docx`).
3. **Full Path:** Canonical absolute Windows path (e.g., `C:\Users\Project\Docs\Architecture_Report_2026.docx`).
4. **Extension:** Canonical lowercase extension without leading dot (`docx`).
5. **File Category:** Mapped from extension table into `FileCategory` enum.
6. **File Size:** 64-bit integer bytes.
7. **Creation Timestamp:** Filesystem creation time (`time_t`).
8. **Modification Timestamp:** Filesystem write time (`time_t`).
9. **MIME/Type:** Fast heuristic mapping from extension.
10. **Extraction Status:** Enumerated state (`Pending`, `Extracted`, `MetadataOnly`, `Failed`, `Truncated`).
11. **Thumbnail Reference:** Relative cache path or placeholder icon identifier.

### 2.4 Universal Metadata Searchability & Multilingual Guarantee
Filename tokens are indexed directly into the `InvertedIndex` for **every** file, tagged with the bitmask `FIELD_FILENAME = 0x01`. Consequently:
- Non-text files (e.g., `Graduation_Ceremony_2025.mp4` or `PCB_Layout.png`) are immediately discoverable by typing `graduation`, `ceremony`, `pcb`, or `layout`.
- Filter queries (e.g., category = `Video`, size $\ge 100\ \text{MB}$, modified after `2025-01-01`) are evaluated against the `FileMetadata` array in $O(1)$ per candidate document.
- **Multilingual & Non-ASCII Guarantee:** Filenames containing non-Latin scripts (e.g., Urdu `اردو_کورس_نوٹس.docx`, Arabic, Cyrillic, CJK, or accented Latin) are tokenized by UTF-8 whitespace and punctuation delimiters. Tokens are hashed byte-by-byte into the `InvertedIndex` using 64-bit FNV-1a. **Searchable filename information is never silently discarded.** Exact-match and Boolean queries over Urdu and multilingual filenames function seamlessly.

---

## 3. System B: Content Indexing System

### 3.1 Content Extraction Strategy & Plugin Architecture
Content extraction is decoupled behind an abstract C++ interface:

```
class IContentExtractor {
public:
    virtual ~IContentExtractor() = default;
    virtual bool supportsExtension(const std::string& ext) const = 0;
    virtual ExtractionResult extractText(const std::string& path, 
                                        std::string& outText, 
                                        size_t maxBytes) = 0;
};
```

### 3.2 Format Extractor Specifications

| Format | Category | Release 1 Engine / Strategy | Configurable Cap (Default: 50 MB) | Fallback Behavior |
| :--- | :--- | :--- | :--- | :--- |
| **Plain Text (`.txt`, `.log`, `.csv`, `.tsv`, `.rtf`)** | Documents | Direct binary file stream reading with BOM-aware UTF-8/UTF-16 decoding | User-configurable (5–100 MB; default 50 MB) | If file size > cap, read up to cap; mark `ExtractionStatus::Truncated` |
| **Markdown (`.md`, `.markdown`)** | Documents | Plain-text parser, stripping Markdown formatting characters | User-configurable (default 50 MB) | Standard text fallback; partial index if > cap |
| **Source Code (`.cpp`, `.h`, `.py`, `.java`, `.html`, `.css`, `.json`, `.xml`, `.cmake`)** | Code | Lexical token stream extractor; splits camelCase and snake_case identifiers | User-configurable (default 50 MB) | Raw ASCII/UTF-8 extraction; partial index if > cap |
| **PDF (`.pdf`)** | Documents | *Interface Defined.* Release 1 stub parser; integration hook for Poppler/PDFium | User-configurable (default 25 MB) | Gracefully falls back to `MetadataOnly` if extractor library is not linked |
| **Microsoft Word (`.docx`)** | Documents | *Interface Defined.* ZIP-container extractor targeting `word/document.xml` | User-configurable (default 25 MB) | Gracefully falls back to `MetadataOnly` if XML/ZIP parser is absent |
| **Microsoft PowerPoint (`.pptx`)** | Documents | *Interface Defined.* ZIP-container extractor targeting `ppt/slides/slide*.xml` | User-configurable (default 25 MB) | Gracefully falls back to `MetadataOnly` |
| **Microsoft Excel (`.xlsx`)** | Documents | *Interface Defined.* ZIP-container extractor targeting `xl/sharedStrings.xml` | User-configurable (default 25 MB) | Gracefully falls back to `MetadataOnly` |
| **Archives (`.zip`, `.7z`, `.rar`, `.tar`, `.gz`)** | Archives | Container metadata and filename indexed only | N/A (Release 1 Scope) | Internal compressed contents are NOT extracted or indexed in Release 1 |

### 3.3 Future Extensibility Hooks (Releases 2–4)
The architecture provides explicit registration hooks in `ContentExtractorRegistry`:
1. **OCR Hook (Tesseract OCR):** For `.png`, `.jpg`, `.bmp`, `.pdf` scans. When linked, the OCR worker extracts recognized text into the standard `TextProcessor` pipeline.
2. **Video Frame Analyzer (OpenCV / FFmpeg):** Extracts keyframe thumbnails, frame subtitles, and stream metadata.
3. **Audio Transcription Hook (Whisper / FFmpeg):** Audio extraction plugin for `.mp3`, `.wav`, `.m4a` files generating timestamped transcript text.

---

## 4. File Discovery & Traversal Engine

### 4.1 Recursive Queue-Based Traversal Algorithm
To avoid call-stack overflow inherent to recursive function calls on deep directory trees ($> 500$ subfolder levels), directory traversal uses an iterative Breadth-First Search (BFS) driven by `CustomQueue<std::string>`:

```
Algorithm TraverseDirectories(rootDirectories, cancellationToken, catalog, index):
    dirQueue = CustomQueue<std::string>()
    visitedInodes = CustomHashTable<uint64_t, bool>() // Cycle suppression

    for each root in rootDirectories:
        if FileSystem::Exists(root) and FileSystem::IsDirectory(root):
            dirQueue.enqueue(FileSystem::Canonical(root))

    while not dirQueue.empty() and not cancellationToken.isRequested():
        currentDir = dirQueue.dequeue()
        
        dirIterator = FileSystem::CreateIterator(currentDir, ec)
        if ec:
            Logger::Warn("Permission denied or directory unreadable: " + currentDir)
            continue

        for each entry in dirIterator:
            if cancellationToken.isRequested(): break

            if entry.is_directory():
                if not IsExcludedDirectory(entry.path()) and not entry.is_symlink():
                    dirQueue.enqueue(entry.path())
            else if entry.is_regular_file():
                ProcessDiscoveredFile(entry, catalog, index)
```

### 4.2 Traversal Resilience & Edge Case Management

#### 1. Multiple Folder & Drive-Level Selection
- Users may select arbitrary combinations of folders (e.g., `C:\Projects`, `D:\Archive`, `E:\Notes`).
- **Ancestor Deduplication:** If user selects both `C:\Projects` and `C:\Projects\SearchEngine`, the scanner detects that the latter is a child of the former and removes the child from the root queue to eliminate redundant traversal.

#### 2. Windows System Directories & Permission Denials
- Windows system folders (`C:\Windows`, `C:\Program Files`, `System Volume Information`, `$Recycle.Bin`, `AppData\Local\Temp`) are protected by OS Access Control Lists (ACLs).
- The scanner uses non-throwing overloads `std::filesystem::directory_iterator(path, ec)`. If `ec` indicates `ERROR_ACCESS_DENIED`, the folder is logged and bypassed without interrupting the scan.

#### 3. Symbolic Links and Infinite Directory Loops
- Symbolic links and directory junctions (common in Windows `C:\Users\User\Application Data -> ...`) can create infinite cycles.
- Traversal checks `entry.is_symlink()`. Subdirectory links are skipped by default; files target canonical paths, preventing cyclic traversal.

#### 4. File Locking & Sharing Violations
- Files opened exclusively by other Windows processes (e.g., active SQLite databases, running Outlook `.pst` files) throw sharing violations (`ERROR_SHARING_VIOLATION`).
- The extractor attempts to open files with `std::ios::in | std::ios::binary` and non-exclusive shared read permissions (`FILE_SHARE_READ`). If opening fails, extraction status is marked `ExtractionStatus::Failed`, and metadata indexing remains intact.

#### 5. Very Large Files & Configurable Extraction Thresholds
- Files exceeding the user-configurable extraction threshold (default **50 MB**, configurable between 5 MB and 100 MB) are processed via streaming:
  - The stream extractor reads up to the threshold limit.
  - The partial text is tokenized and indexed into the Inverted Index.
  - The document record is assigned `ExtractionStatus::Truncated`.
- Files exceeding 1 GB are automatically restricted to metadata-only indexing (`ExtractionStatus::MetadataOnly`), preserving absolute memory safety.

---

## 5. Stable Document ID (DocID) Strategy, Change Detection & Tombstone Consistency

### 5.1 Dual Document Identity Model
The system enforces a strict separation between persistent identity and runtime memory indexation:
1. **Persistent Identity:** The canonical absolute filesystem path (`std::string`) stored within the `DocumentCatalog` contiguous string pool, indexed via a 64-bit FNV-1a hash table for $O(1)$ duplicate and change detection.
2. **Dense In-Memory DocID:** A 32-bit unsigned integer ($1, 2, 3, \dots, N$) that acts as a direct $O(1)$ array offset into `CustomDynamicArray<FileMetadata>`:
   $$\text{MetadataPointer} = \text{m\_catalogData}[\text{docId} - 1]$$
- `DocID = 0` is strictly reserved as an invalid sentinel value (`INVALID_DOC_ID`).

### 5.2 Monotonic ID Assignment & Runtime No-Reuse Policy
- During an active application session, `DocID` values are strictly monotonic: `nextDocId = ++m_highestDocId`.
- **No-Reuse Rule:** `DocID` values are **never recycled or reused at runtime**. If file `DocID 15` is modified or deleted, `15` is tombstoned and permanently retired for that session. This prevents race conditions, eliminates stale pointer traps, and ensures that concurrent query evaluations never inspect reallocated metadata entries.

### 5.3 Fast Incremental Change Detection
To verify whether an existing index is stale without re-reading all files on disk, `FileScanner` executes an $O(1)$ attribute comparison per file:

```
Algorithm DetectFileDelta(storedMetadata, currentFsEntry):
    if not currentFsEntry.exists():
        return FileDeltaState::Deleted
    
    if currentFsEntry.last_write_time != storedMetadata.lastModifiedTime:
        return FileDeltaState::Modified
        
    if currentFsEntry.file_size != storedMetadata.fileSize:
        return FileDeltaState::Modified

    return FileDeltaState::Unchanged
```

### 5.4 Handling Deleted and Modified Files (Soft Tombstoning)
In-place removal of a `DocID` from thousands of posting lists would require $O(V \times L)$ linked list traversals. To achieve sub-second updates:
1. **Deleted Files:**
   - The file's record in `DocumentCatalog` sets `flags.is_active = 0` (tombstoned).
   - The path-to-DocID hash entry is removed.
   - When Boolean search evaluates postings, candidates with `is_active == 0` are filtered out in $O(1)$.
2. **Modified Files:**
   - The old `DocID` is tombstoned (`is_active = 0`).
   - The updated file is assigned a fresh monotonic `DocID = ++m_highestDocId`.
   - New metadata and newly extracted content postings are appended to the index.

### 5.5 Tombstone Compaction & Reference Remapping Algorithm
During explicit index maintenance or index persistence save, tombstoned entries are purged and `DocID`s are compacted to eliminate sparse gaps. To guarantee that **zero posting references become invalid or dangling**, the compactor executes a two-pass reference remapping:
1. **Pass 1 (Remap Generation):** An array `remapTable[oldDocId]` is generated. Active documents receive contiguous IDs $1 \dots N_{active}$; tombstoned entries are marked `0`.
2. **Pass 2 (Posting Rewrite):** Every `CustomPostingList` in the `InvertedIndex` is traversed. Posting nodes pointing to `remapTable[docId] == 0` are unlinked and deallocated; surviving nodes have their `docId` updated to `remapTable[docId]`.
3. **Result:** The `DocumentCatalog` array is compacted, all tombstoned entries are reclaimed, and all posting lists maintain strict ascending `DocID` order with 100% valid document references.
