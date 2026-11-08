# Software Requirements Specification (SRS)
## Project: Universal Smart Search Engine
### Release 1 — Local Universal File Search Engine
**Target Platform:** Windows Desktop (C++17 / MSVC 2022 x64 / Qt 6 Widgets / CMake)  
**Academic Context:** University Data Structures & Algorithms Coursework  
**Document Version:** 1.0.0  
**Status:** Approved for Architecture & DSA Baseline  

---

## 1. Introduction

### 1.1 Project Overview
The **Universal Smart Search Engine (Release 1 — Local Universal File Search Engine)** is a high-performance, native Windows desktop application designed to discover, index, rank, and retrieve local files across user-specified directories and storage drives. Built from the ground up in modern C++17 and Qt 6 Widgets, the application bridges the gap between high-level desktop usability and fundamental computer science principles. Rather than relying on external black-box search engines or database abstraction layers, the core indexing, querying, autocomplete, and relevance-scoring pipelines are driven by custom-engineered Data Structures and Algorithms (DSA).

### 1.2 Purpose
The purpose of this document is to specify the complete functional, non-functional, interface, and operational requirements for Release 1 of the Universal Smart Search Engine. It serves as the authoritative contract and baseline for software architects, C++ engineers, UI designers, and university academic evaluators.

### 1.3 Scope
The scope of Release 1 encompasses:
- Local drive and nested directory discovery.
- Comprehensive file classification across seven major file categories.
- Dual-tier indexing: Fast Metadata Indexing for all discoverable files, and Deep Full-Text Indexing for extractable plain text, documents, and source code.
- Boolean query parsing (`AND`, `OR`, `NOT`, and parenthetical grouping).
- Term Frequency-Inverse Document Frequency (TF-IDF) relevance scoring coupled with a custom Max-Heap for Top-$K$ ranked result retrieval.
- Prefix-based autocomplete powered by a custom Trie with frequency weighting.
- Edit-distance based spelling suggestions for search terms.
- Local persistent index storage, serialization, change detection, and incremental rebuilds.
- Native Qt 6 GUI with multi-threaded background scanning/indexing, search dashboard, file preview panes, and keyword highlighting.

### 1.4 Objectives
1. **Academic Rigor:** Implement all foundational search and retrieval structures (Hash Table, Posting Lists, Trie, Max-Heap, Stack, Queue, Dynamic Array, Singly Linked List) from first principles in C++17 without relying on standard template library (STL) containers for core indexing structures.
2. **Deterministic Performance:** Achieve sub-50 ms search query response times for multi-word Boolean queries over an index of 100,000 files on standard consumer PC hardware.
3. **GUI Responsiveness:** Guarantee zero UI thread freezing during long-running recursive file discovery and content extraction through asynchronous worker architectures.
4. **Architectural Separation:** Strict decoupling between presentation (Qt Widgets), business logic/service layer, and custom algorithmic data structures.

### 1.5 Intended Users
- **University Course Evaluators & Faculty:** Assessing the algorithmic correctness, time/space efficiency, memory management, and OOP architectural integrity of custom data structures.
- **Power Users & Developers:** Seeking an instant, local, offline desktop file retrieval tool capable of searching codebases, notes, PDFs, and deep folder structures with Boolean and ranked queries.
- **General Desktop Users:** Requiring a unified search utility to locate files by name, type, size, date, or internal text with previewing capabilities.

### 1.6 Definitions, Acronyms, and Terminology

| Term / Acronym | Definition |
| :--- | :--- |
| **Document ID (DocID)** | A unique 32-bit unsigned integer (`uint32_t`) assigned sequentially to each discovered file catalog entry. |
| **Posting** | A record in an inverted index indicating that a specific term occurs in a specific document, storing `DocID` and Term Frequency (`TF`). |
| **Posting List** | A monotonically increasing sorted linked list of postings associated with a specific vocabulary term. |
| **Inverted Index** | A mapping structure (implemented via a custom Hash Table with separate chaining) associating normalized vocabulary terms with their respective Posting Lists. |
| **Metadata Index** | A structured repository mapping `DocID`s to physical file attributes (filename, path, size, timestamps, category, extension). |
| **TF (Term Frequency)** | The raw or normalized count of occurrences of a term within a specific document. |
| **IDF (Inverse Document Frequency)** | An information retrieval metric reflecting how common or rare a term is across the entire indexed corpus: $\log\left(1 + \frac{N}{DF}\right)$. |
| **TF-IDF** | The product of $TF$ and $IDF$, representing the statistical importance of a term within a specific document relative to the corpus. |
| **Top-$K$ Retrieval** | The process of extracting only the highest $K$ ranked documents using a Max-Heap priority queue without sorting the entire candidate set. |
| **Trie** | A digital search tree where nodes represent character transitions, used here for prefix matching and query autocomplete. |
| **Shunting-Yard Algorithm** | Dijkstra's stack-based algorithm used to parse infix Boolean query expressions into Reverse Polish Notation (RPN). |
| **MIME Type** | Multipurpose Internet Mail Extensions string identifying the format of a file (e.g., `application/pdf`, `text/plain`). |
| **Stop Words** | Extremely common words (e.g., "the", "is", "at") filtered out during tokenization to preserve index compactness and retrieval quality. |

---

## 2. Product Scope

### 2.1 Universal Search Paradigm
The application is "universal" in its file discovery reach: it does not restrict indexing to arbitrary system library folders. Users can target entire storage drives (e.g., `C:\`, `D:\`) or arbitrary directory trees. Every file system entry matching OS accessibility constraints is cataloged.

### 2.2 Local File Discovery Mechanism
Discovery is performed recursively using a custom Breadth-First Search (BFS) or Depth-First Search (DFS) queue-driven file scanner. The scanner interacts directly with standard C++17 filesystem interfaces (`std::filesystem`), bypassing Windows shell indexers to maintain total transparency, deterministic indexing, and cross-drive flexibility.

### 2.3 The Critical Architectural Distinction: Metadata vs. Content Search
A core tenet of the Universal Smart Search Engine is the clean separation of indexing modalities:

```
                          ┌──────────────────────────────┐
                          │   Discovered File System     │
                          │           Entry              │
                          └──────────────┬───────────────┘
                                         │
                         Extract Metadata (Universal)
                         • Name, Path, Ext, Size, Dates
                                         │
                                         ▼
                          ┌──────────────────────────────┐
                          │    Document Catalog Entry    │
                          │        (All Files)           │
                          └──────────────┬───────────────┘
                                         │
                    Is File Format Supported for Text Extraction?
                                         │
                        ┌────────────────┴────────────────┐
                        │ YES                             │ NO
                        ▼                                 ▼
         ┌──────────────────────────────┐  ┌──────────────────────────────┐
         │   Content Extractor Engine   │  │   Metadata-Only Fallback     │
         │ (TXT, Code, PDF, DOCX, etc.) │  │  (Images, Video, Audio,      │
         └──────────────┬───────────────┘  │   Archives, System Binaries) │
                        │                  └──────────────┬───────────────┘
               Extraction Successful?                     │
                        │                                 │
                 ┌──────┴──────┐                          │
                 ▼             ▼                          │
             [Content]     [Fallback]                     │
                 │             │                          │
                 └──────┬──────┘                          │
                        │                                 │
                        ▼                                 ▼
             Full-Text Inverted Index             Filename & Attribute
             (Metadata + Extracted Text)          Index Only
```

1. **Metadata Indexing (100% Corpus Coverage):** Every single file discovered—whether an uncompressed plain text note, a high-resolution PNG image, an MP4 video, a compiled DLL, or an encrypted archive—is parsed for filesystem metadata (filename, directory path, extension, byte size, creation timestamp, last modification timestamp, category). Filename tokens are indexed into the Inverted Index for all files.
2. **Full-Text Content Indexing (Selective Coverage):** Full-text content parsing is executed **only** when a valid, dedicated content extraction plugin/routine is available for that file's format and the file can be safely read. If extraction fails, is unsupported, or encounters binary encryption, the file automatically degrades gracefully to Metadata-Only indexing.

### 2.4 Supported File Categories and Indexing Capabilities

| Category | Typical Extensions | Metadata Search | Content Search (Release 1) | Notes / Extractor Strategy |
| :--- | :--- | :---: | :---: | :--- |
| **Documents** | `.txt`, `.pdf`, `.docx`, `.doc`, `.pptx`, `.xlsx`, `.md`, `.rtf` | Yes | Yes (TXT, MD; PDF & DOCX via extensible extractor interfaces) | TXT/MD natively indexed. Office documents (.docx/.pptx/.xlsx) parsed via ZIP container + XML stream reader; PDF parsed if extractor library linked, otherwise gracefully falls back to `MetadataOnly`. |
| **Source Code** | `.cpp`, `.h`, `.hpp`, `.py`, `.java`, `.html`, `.css`, `.js`, `.json`, `.xml`, `.cmake` | Yes | Yes | Natively tokenized and indexed using UTF-8 text processor. |
| **Images** | `.jpg`, `.jpeg`, `.png`, `.webp`, `.gif`, `.bmp`, `.svg` | Yes | No (Metadata Only) | Future OCR hook reserved. Dimensions/metadata stored where available. Filename tokens 100% indexed. |
| **Videos** | `.mp4`, `.mkv`, `.avi`, `.mov`, `.webm` | Yes | No (Metadata Only) | Future FFmpeg hook reserved. Discoverable by title, date, size, path. |
| **Audio** | `.mp3`, `.wav`, `.aac`, `.flac`, `.m4a` | Yes | No (Metadata Only) | Future transcription hook reserved. Discoverable by filename metadata. |
| **Archives** | `.zip`, `.7z`, `.rar`, `.tar`, `.gz` | Yes | No (Release 1 Scope) | Container filename and metadata are fully indexed. Internal compressed contents are NOT extracted or indexed in Release 1 (reserved for future release). |
| **Other / Binaries** | `.exe`, `.dll`, `.sys`, `.bin`, `.iso`, `.dat` | Yes | No (Metadata Only) | Binary content skipped; filename and filesystem metadata fully indexed. |

### 2.5 Release 1 Limitations, Configurable Thresholds & Multilingual Support
- **Local Scope:** Release 1 is strictly an offline local search engine.
- **Remote / Cloud Scope:** No remote network shares or cloud drives requiring web authentication (locally mounted physical folders are scanned).
- **Multilingual & Unicode Invariant:** Filenames in any language or script (e.g., Urdu, Arabic, Cyrillic, CJK, accented Latin) are tokenized and hashed byte-by-byte into the `InvertedIndex` via raw UTF-8 64-bit FNV-1a. **Zero filename or metadata information is silently discarded.**
- **Configurable Extraction Threshold:** Files exceeding a user-configurable size threshold (default **50 MB**, configurable between 5 MB and 100 MB in Settings) have their text stream indexed up to the limit and their status set to `ExtractionStatus::Truncated`. Files exceeding 1 GB are automatically indexed as `MetadataOnly`.

---

## 3. Functional Requirements

### 3.1 Module 1: File Discovery & Scanning (FDS)

#### FR-FDS-001: Single Folder Selection
- **Description:** The system shall allow the user to select a single root directory from the local filesystem via a native Windows folder picker dialog.
- **Input:** User directory selection via GUI.
- **Expected Output:** Absolute directory path validated and staged for indexing.
- **Priority:** High (P1).
- **Dependencies:** None.
- **Acceptance Criteria:** Path must exist, be readable under current user permissions, and appear in the indexed source list.

#### FR-FDS-002: Multiple Folder Selection
- **Description:** The system shall allow the user to specify multiple distinct folder paths simultaneously across different directory branches.
- **Input:** List of directory paths.
- **Expected Output:** Aggregated root path list without duplicate overlapping ancestors.
- **Priority:** High (P1).
- **Dependencies:** FR-FDS-001.
- **Acceptance Criteria:** Overlapping child paths (e.g., selecting `C:\A` and `C:\A\B`) must be normalized or deduplicated to avoid redundant traversal.

#### FR-FDS-003: Drive-Level Selection
- **Description:** The system shall allow the user to select an entire logical drive root (e.g., `C:\`, `D:\`) for scanning.
- **Input:** Drive letter selection.
- **Expected Output:** Root drive path staged with elevated permission warnings where appropriate.
- **Priority:** Medium (P2).
- **Dependencies:** FR-FDS-001.
- **Acceptance Criteria:** System skips inaccessible OS-protected system directories (e.g., `System Volume Information`) gracefully without halting traversal.

#### FR-FDS-004: Recursive Directory Traversal
- **Description:** The file scanner shall recursively traverse all subdirectories beneath selected roots using a breadth-first queue algorithm.
- **Input:** Validated root directory queue.
- **Expected Output:** Exhaustive stream of file path entries discovered.
- **Priority:** High (P1).
- **Dependencies:** FR-FDS-001, FR-FDS-002.
- **Acceptance Criteria:** All non-hidden, non-system files down to arbitrary directory depths are cataloged.

#### FR-FDS-005: File Classification
- **Description:** The scanner shall classify every discovered file into exactly one of the standard categories (Documents, Code, Images, Videos, Audio, Archives, Other) based on its canonical file extension.
- **Input:** File extension string.
- **Expected Output:** `FileCategory` enumeration tag assigned to the file metadata.
- **Priority:** High (P1).
- **Dependencies:** FR-FDS-004.
- **Acceptance Criteria:** Extension parsing must be case-insensitive (e.g., `.PDF` == `.pdf`). Unrecognized extensions map to `FileCategory::Other`.

#### FR-FDS-006: Duplicate Detection
- **Description:** The system shall detect duplicate files encountered during scanning across overlapping paths or distinct locations.
- **Input:** File absolute path and file size/modification timestamp.
- **Expected Output:** Suppression of duplicate indexing for identical canonical paths; tracking of identical content hashes if hash-checking is enabled.
- **Priority:** Medium (P2).
- **Dependencies:** FR-FDS-004.
- **Acceptance Criteria:** Canonical filesystem symlinks or duplicate folder inclusions do not generate duplicate DocIDs.

#### FR-FDS-007: Traversal Error Handling & Permission Resilience
- **Description:** The scanner shall catch and log filesystem errors (access denied, path too long, broken symlinks, locked files) without crashing or terminating the scan.
- **Input:** Filesystem exception or error code during iteration.
- **Expected Output:** Warning logged to error log, problematic file/folder skipped, scanner resumes next entry.
- **Priority:** High (P1).
- **Dependencies:** FR-FDS-004.
- **Acceptance Criteria:** Zero crashes when traversing locked Windows system files or restricted folders.

---

### 3.2 Module 2: Metadata Extraction & Cataloging (MEC)

#### FR-MEC-001: File Attribute Extraction
- **Description:** The system shall extract standard filesystem metadata for every discovered file.
- **Input:** Valid file path.
- **Expected Output:** Populated `FileMetadata` record containing:
  1. Base filename.
  2. File extension.
  3. Canonical absolute path.
  4. File size in bytes.
  5. `FileCategory` enum.
  6. Creation timestamp (`time_t` or ISO 8601).
  7. Last modification timestamp (`time_t` or ISO 8601).
  8. Extraction status enum (`Pending`, `Extracted`, `MetadataOnly`, `Failed`).
- **Priority:** High (P1).
- **Dependencies:** FR-FDS-004, FR-FDS-005.
- **Acceptance Criteria:** All attributes populated within $50\ \mu\text{s}$ per file; missing timestamps fall back to epoch zero.

#### FR-MEC-002: Document Catalog Assignment
- **Description:** The system shall assign a unique sequential 32-bit unsigned integer `DocID` starting at `1` to every unique discovered file and store it in the `DocumentCatalog`.
- **Input:** `FileMetadata` object.
- **Expected Output:** Assigned `DocID` and insertion into `DocumentCatalog` (Custom Dynamic Array).
- **Priority:** High (P1).
- **Dependencies:** FR-MEC-001.
- **Acceptance Criteria:** `DocID` remains stable throughout the active application session and is bijective with the file path.

#### FR-MEC-003: Filename Token Indexing
- **Description:** The system shall tokenize the base filename (excluding extension) using alphanumeric boundaries and delimiters (`_`, `-`, `.`, camelCase splits) and index these tokens into the Inverted Index with elevated metadata weight.
- **Input:** Filename string and assigned `DocID`.
- **Expected Output:** Filename terms inserted into Inverted Index pointing to `DocID`.
- **Priority:** High (P1).
- **Dependencies:** FR-MEC-002.
- **Acceptance Criteria:** Searching for a word in a filename returns that document even if the file is an image or video with no internal text content.

---

### 3.3 Module 3: Content Extraction (CEX)

#### FR-CEX-001: Plain Text & Markdown Extraction
- **Description:** The system shall extract text content from standard plain text files (`.txt`, `.md`, `.rtf`, `.log`, `.csv`).
- **Input:** File path.
- **Expected Output:** Stream or buffer of UTF-8 encoded plain text.
- **Priority:** High (P1).
- **Dependencies:** FR-MEC-002.
- **Acceptance Criteria:** Correctly decodes ASCII, UTF-8, and UTF-16 (with BOM) files without garbled characters.

#### FR-CEX-002: Source Code Extraction
- **Description:** The system shall read source code files (`.cpp`, `.h`, `.hpp`, `.py`, `.java`, `.html`, `.css`, `.js`, etc.) as raw text, treating language keywords and identifiers as indexable tokens.
- **Input:** File path.
- **Expected Output:** Normalized character stream representing code tokens.
- **Priority:** High (P1).
- **Dependencies:** FR-MEC-002.
- **Acceptance Criteria:** Handles syntax characters cleanly; splits snake_case and camelCase identifiers where configured.

#### FR-CEX-003: Document Extraction (PDF, Office Formats)
- **Description:** The system shall provide an extensible extractor interface (`IContentExtractor`) supporting PDF and Microsoft Office formats (`.docx`, `.pptx`, `.xlsx`). In Release 1, when native binary parsers/libraries are linked, text streams are extracted; otherwise, graceful stubbing marks status as `MetadataOnly`.
- **Input:** Document file path.
- **Expected Output:** Extracted text buffer, or explicit error code triggering fallback.
- **Priority:** Medium (P2).
- **Dependencies:** FR-MEC-002.
- **Acceptance Criteria:** If extraction fails or library is absent, the file's `extraction_status` is updated to `MetadataOnly` or `Failed`, and scanning continues uninterrupted.

#### FR-CEX-004: Extraction Failure and Size Thresholding
- **Description:** If a file cannot be read (locked by another process, corrupted structure, unsupported encoding, or exceeds maximum file size limit of 50 MB), the extractor must fail safely.
- **Input:** Problematic file path.
- **Expected Output:** Extraction status set to `Failed` or `Truncated`; partial text indexed if truncated; warning recorded in extraction statistics.
- **Priority:** High (P1).
- **Dependencies:** FR-CEX-001 to FR-CEX-003.
- **Acceptance Criteria:** System never crashes due to a corrupt PDF, binary garbage disguised as text, or unreadable files.

---

### 3.4 Module 4: Text Processing & Inverted Indexing (TPI)

#### FR-TPI-001: Tokenization and Query Normalization
- **Description:** The text processor shall split raw text streams into discrete lexical tokens using non-alphanumeric delimiters.
- **Input:** Raw text string.
- **Expected Output:** Ordered list of normalized tokens (lowercase, stripped of punctuation).
- **Priority:** High (P1).
- **Dependencies:** FR-CEX-001, FR-CEX-002.
- **Acceptance Criteria:** All tokens converted to lowercase ASCII/UTF-8; punctuation stripped; tokens shorter than 2 characters (e.g., single letters) or longer than 45 characters discarded.

#### FR-TPI-002: Stop-Word Filtering
- **Description:** The system shall filter out standard English stop words (e.g., "a", "an", "the", "and", "or", "in", "on", "of", "to") during indexing to conserve memory and avoid ranking distortion.
- **Input:** Normalized token stream.
- **Expected Output:** Filtered token stream containing only content terms.
- **Priority:** High (P1).
- **Dependencies:** FR-TPI-001.
- **Acceptance Criteria:** Filtering checked against a fast custom hash set or lookup array in $O(1)$ average time per token.

#### FR-TPI-003: Posting List Insertion & Maintenance
- **Description:** For every valid term extracted from document `DocID`, the system shall update the Inverted Index: locate or insert the term into the custom Hash Table, and append or increment the term frequency in that document's Posting in the Posting List.
- **Input:** Token, `DocID`.
- **Expected Output:** Updated Inverted Index where posting lists maintain ascending `DocID` order.
- **Priority:** High (P1).
- **Dependencies:** FR-TPI-001, FR-TPI-002.
- **Acceptance Criteria:** Each posting accurately records $(DocID, TermFrequency)$. No duplicate postings for the same $(Term, DocID)$ pair.

---

### 3.5 Module 5: Search Engine & Boolean Query Processing (BQP)

#### FR-BQP-001: Single-Word Query Execution
- **Description:** The system shall accept a single search term, normalize it, perform a hash lookup in the Inverted Index, and return the matching Posting List.
- **Input:** Single-word string (e.g., `algorithm`).
- **Expected Output:** Set of matching `DocID`s with their associated $TF$ values.
- **Priority:** High (P1).
- **Dependencies:** FR-TPI-003.
- **Acceptance Criteria:** Exact lookup completed in under $1\ \text{ms}$ for an index of 100,000 documents.

#### FR-BQP-002: Implicit Multi-Word Query (Default AND)
- **Description:** When the user enters multiple terms without explicit Boolean operators (e.g., `data structures lecture`), the system shall treat the query as an implicit conjunction (`data AND structures AND lecture`).
- **Input:** Multi-word string.
- **Expected Output:** Intersected result set containing documents matching all terms.
- **Priority:** High (P1).
- **Dependencies:** FR-BQP-001.
- **Acceptance Criteria:** Returns only documents containing all entered keywords.

#### FR-BQP-003: Explicit Boolean Operators (`AND`, `OR`, `NOT`)
- **Description:** The system shall parse explicit Boolean operators:
  - `AND`: Set intersection of postings.
  - `OR`: Set union of postings.
  - `NOT`: Set difference relative to the universe of indexed documents.
- **Input:** Query string with uppercase or lowercase operators (e.g., `report AND 2024 NOT draft`).
- **Expected Output:** Evaluated candidate set of `DocID`s.
- **Priority:** High (P1).
- **Dependencies:** FR-BQP-001, FR-BQP-002.
- **Acceptance Criteria:** Boolean operations correctly adhere to standard set theory logic.

#### FR-BQP-004: Parenthetical Grouping & Precedence
- **Description:** The system shall support arbitrary parenthetical expressions to override default operator precedence using a custom Stack-based Shunting-Yard algorithm.
- **Precedence Order:** Parentheses `()` > `NOT` > `AND` > `OR`.
- **Input:** Complex query (e.g., `(c++ OR rust) AND (concurrency NOT mutex)`).
- **Expected Output:** Evaluated candidate set matching the exact logical expression.
- **Priority:** High (P1).
- **Dependencies:** FR-BQP-003.
- **Acceptance Criteria:** Mismatched or unbalanced parentheses trigger an informative syntax error message in the GUI without crashing.

#### FR-BQP-005: Search Filtering
- **Description:** The system shall allow filtering the evaluated search results by:
  1. `FileCategory` (e.g., only Documents or only Images).
  2. File extension (e.g., only `.cpp`).
  3. File size range (min bytes, max bytes).
  4. Date modified range (after date, before date).
- **Input:** Active search query and filter criteria flags.
- **Expected Output:** Filtered candidate set satisfying both the query and metadata constraints.
- **Priority:** High (P1).
- **Dependencies:** FR-BQP-003, FR-MEC-001.
- **Acceptance Criteria:** Filtering occurs efficiently by validating metadata records in $O(1)$ per candidate document.

---

### 3.6 Module 6: Ranking & Top-K Scoring (RNK)

#### FR-RNK-001: TF-IDF Score Calculation
- **Description:** For every candidate document satisfying the Boolean query expression, the system shall compute its composite relevance score using the standard TF-IDF formulation:
  $$\text{Score}(d, Q) = \sum_{t \in Q \cap d} \left(1 + \ln(TF_{t, d})\right) \times \ln\left(1 + \frac{N}{DF_t}\right) \times W_{field}$$
  where $W_{field}$ provides elevated weight if the term appears in the filename versus the body text.
- **Input:** Candidate document list and query terms.
- **Expected Output:** Calculated floating-point score for each document.
- **Priority:** High (P1).
- **Dependencies:** FR-BQP-003, FR-MEC-003.
- **Acceptance Criteria:** Documents with frequent occurrences of rare query terms rank strictly higher than documents with common terms.

#### FR-RNK-002: Top-$K$ Heap-Based Retrieval
- **Description:** Instead of sorting the entire candidate set ($O(M \log M)$ where $M$ is the number of matching documents), the system shall utilize a custom Max-Heap to extract exactly the top $K$ highest-scoring results (default $K = 50$, configurable up to $1,000$).
- **Input:** Set of scored `(DocID, Score)` pairs and integer $K$.
- **Expected Output:** Sorted array of the top $K$ `(DocID, Score)` records in descending score order.
- **Priority:** High (P1).
- **Dependencies:** FR-RNK-001.
- **Acceptance Criteria:** Heap selection completes in $O(M + K \log M)$ time. Tie-breaking rules prioritize more recent file modification dates, followed by smaller `DocID`.

---

### 3.7 Module 7: Intelligent Search Assistance (ISA)

#### FR-ISA-001: Prefix Autocomplete via Custom Trie
- **Description:** As the user types into the search bar, the system shall query a custom Trie populated with the indexed corpus vocabulary and return up to 5 prefix-matching term suggestions.
- **Input:** Search prefix string (minimum 2 characters).
- **Expected Output:** List of up to 5 autocomplete word suggestions.
- **Priority:** High (P1).
- **Dependencies:** FR-TPI-001.
- **Acceptance Criteria:** Autocomplete results return within $5\ \text{ms}$ on keystroke events.

#### FR-ISA-002: Frequency-Weighted Suggestions
- **Description:** Autocomplete suggestions shall be ordered by historical search frequency and corpus occurrence frequency so that high-frequency terms appear first.
- **Input:** Prefix matching candidate nodes.
- **Expected Output:** Sorted suggestion list prioritizing popular terms.
- **Priority:** Medium (P2).
- **Dependencies:** FR-ISA-001.
- **Acceptance Criteria:** Words with higher query/document frequency rank above obscure words with the same prefix.

#### FR-ISA-003: Spell Correction & "Did You Mean?" Suggestions
- **Description:** If a query term yields zero or extremely low search hits, the system shall compute the Levenshtein distance (or Damerau-Levenshtein distance $\le 2$) against the Trie vocabulary and suggest the closest valid alternative.
- **Input:** Misspelled search token (e.g., `algoritm`).
- **Expected Output:** Suggested correction string (e.g., `algorithm`).
- **Priority:** Medium (P2).
- **Dependencies:** FR-ISA-001.
- **Acceptance Criteria:** Returns sensible suggestions for single-character deletions, insertions, substitutions, and transpositions within $20\ \text{ms}$.

#### FR-ISA-004: Persistent Search History
- **Description:** The system shall record past search queries with timestamps, query text, result counts, and execution durations in a custom history structure, persisted to disk.
- **Input:** Executed query event.
- **Expected Output:** History entry appended; recent history accessible via UI dropdown.
- **Priority:** Low (P3).
- **Dependencies:** FR-BQP-001.
- **Acceptance Criteria:** Stores at least the last 100 queries; duplicates bump the timestamp without creating redundant entries.

---

### 3.8 Module 8: Graphical User Interface & File Preview (GUI)

#### FR-GUI-001: Unified Search & Filter Dashboard
- **Description:** The GUI shall feature a central search bar, interactive category filter tabs (All, Documents, Code, Images, Videos, Audio, Archives), file size/date range pickers, and indexing status indicators.
- **Input:** User interaction events.
- **Expected Output:** Real-time visual feedback, responsive filtering.
- **Priority:** High (P1).
- **Dependencies:** None.
- **Acceptance Criteria:** Modern, high-DPI aware Qt 6 Widgets interface adhering to desktop styling guidelines.

#### FR-GUI-002: Ranked Result Table/List View
- **Description:** The results view shall present ranked matches displaying: File Name, Category Icon, Relevance Score / Match Percentage, Containing Folder Path, File Size, and Modification Date.
- **Input:** Top-$K$ search results from engine.
- **Expected Output:** Populated Qt Table/List view with column sorting.
- **Priority:** High (P1).
- **Dependencies:** FR-RNK-002.
- **Acceptance Criteria:** Double-clicking an entry triggers the OS default application or native preview.

#### FR-GUI-003: Integrated Document & Code Viewer with Keyword Highlighting
- **Description:** Selecting a text, markdown, or code result shall display its content in a preview side-pane, automatically jumping to and highlighting matching query terms.
- **Input:** Selected `DocID` and active query terms.
- **Expected Output:** Rendered preview text with yellow-highlighted keyword matches and hit-count summary.
- **Priority:** High (P1).
- **Dependencies:** FR-CEX-001, FR-GUI-002.
- **Acceptance Criteria:** Highlighting renders without modifying the underlying file; large files are loaded on demand.

#### FR-GUI-004: Integrated Media Preview Panes
- **Description:** The system shall display image previews (thumbnail and full-res scaling via `QPixmap`) and basic audio/video metadata summaries (or playback via `QMediaPlayer` where supported).
- **Input:** Selected media `DocID`.
- **Expected Output:** Rendered image or media playback widget in preview dock.
- **Priority:** Medium (P2).
- **Dependencies:** FR-GUI-002.
- **Acceptance Criteria:** Unsupported or corrupt media files display an informative placeholder icon rather than crashing.

#### FR-GUI-005: Index Statistics & Telemetry Dashboard
- **Description:** The GUI shall provide a dedicated statistics modal or dock panel presenting: total files cataloged, total vocabulary terms, total postings, index size on disk, memory footprint, breakdown by file category, and last scan duration.
- **Input:** Query to `StatisticsManager`.
- **Expected Output:** Formatted telemetry metrics and charts/bars.
- **Priority:** Medium (P2).
- **Dependencies:** FR-TPI-003, FR-MEC-002.
- **Acceptance Criteria:** Telemetry updates immediately upon scan completion.

---

### 3.9 Module 9: Index Persistence & Lifecycle Management (IPM)

#### FR-IPM-001: Index Serialization (Save to Disk)
- **Description:** The system shall serialize the `DocumentCatalog`, `InvertedIndex` vocabulary and posting lists, and Trie frequencies into a structured binary or compact text format on disk (`.idx` / `.dat`).
- **Input:** Save trigger (manual or clean application exit).
- **Expected Output:** Valid persisted index files written to the application data directory.
- **Priority:** High (P1).
- **Dependencies:** FR-TPI-003, FR-MEC-002.
- **Acceptance Criteria:** Binary file includes magic header (`0x534D5254` "SMRT"), version byte, CRC32 checksum, document catalog block, and inverted index block.

#### FR-IPM-002: Index Deserialization (Load from Disk)
- **Description:** Upon application startup, the system shall read, validate, and deserialize the stored index files into memory without requiring a full filesystem rescan.
- **Input:** Path to index file on startup.
- **Expected Output:** Restored Inverted Index, Document Catalog, and Trie ready for queries.
- **Priority:** High (P1).
- **Dependencies:** FR-IPM-001.
- **Acceptance Criteria:** Complete deserialization of 100,000 files in under 3 seconds. Corrupted checksum triggers rebuild warning.

#### FR-IPM-003: Modified & Deleted File Detection (Incremental Update)
- **Description:** The system shall support incremental re-indexing: comparing current filesystem modification timestamps and existence against the stored catalog.
  - Deleted files: Marked inactive or removed from postings.
  - Modified files: Re-extracted and postings updated.
  - New files: Appended with new `DocID`.
- **Input:** Re-index command for existing indexed roots.
- **Expected Output:** Updated index reflecting current filesystem state without full rebuild.
- **Priority:** High (P1).
- **Dependencies:** FR-IPM-002, FR-FDS-004.
- **Acceptance Criteria:** Modifying 1 file out of 10,000 re-indexes only that single file in under $100\ \text{ms}$.

#### FR-IPM-004: Index Corruption Recovery
- **Description:** If an index file fails checksum validation, has truncated bytes, or encounters read errors during deserialization, the system shall notify the user and offer a one-click automatic clean rebuild.
- **Input:** Corrupted index file on startup.
- **Expected Output:** Error dialog, safe fallback to clean memory state, trigger full rescan.
- **Priority:** High (P1).
- **Dependencies:** FR-IPM-002.
- **Acceptance Criteria:** Zero unhandled exceptions or segfaults on malformed index input.

---

## 4. Non-Functional Requirements (NFR)

All non-functional targets defined below are **Proposed Engineering Targets** subject to formal verification during Phase 10 benchmarking. They represent design goals rather than proven guarantees.

### 4.1 Benchmark Environment & Measurement Protocol
To ensure repeatability and avoid anecdotal performance claims, all metrics are evaluated under the following standardized testing specification:
- **Reference Hardware:** 6-Core / 12-Thread x64 CPU @ 2.5 GHz+ (e.g., AMD Ryzen 5 or Intel Core i5/i7), 16 GB DDR4/DDR5 RAM, PCIe NVMe M.2 SSD, Windows 10/11 64-bit.
- **Corpus Characteristics:** Synthetic standardized test corpus of 100,000 files:
  - 60% plain text and source code files (average size 45 KB).
  - 25% images and media files (metadata indexed only).
  - 15% other formats (PDF, Office stubs, archives).
  - Vocabulary: Approximately 50,000 distinct normalized terms.
- **Cache Definition:**
  - *Cold Cache:* Application launch from clean state; index loaded from disk without prior filesystem buffering.
  - *Warm Cache:* Repeated queries or incremental scans where operating system file table cache and memory index are fully resident.
- **Measurement Methodology:** High-resolution timing via `std::chrono::high_resolution_clock`. Each benchmark must execute for a **minimum of 5 repetitions**, discarding minimum and maximum outliers, reporting the median $\pm$ standard deviation.

### 4.2 Proposed NFR Targets Table

| ID | Category | Metric / Requirement | Proposed Target Value | Testing Conditions | Verification Method |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **NFR-001** | Performance | Multi-word Boolean query search latency (100k files) | $\le 50\ \text{ms}$ (Proposed Target) | Warm in-memory index, 3-term boolean query | Microsecond benchmark timer (median of 5 runs) |
| **NFR-002** | Performance | Single-word exact index lookup latency | $\le 5\ \text{ms}$ (Proposed Target) | Warm memory, FNV-1a hash table lookup | Custom benchmark harness |
| **NFR-003** | Performance | Autocomplete prefix suggestion response time | $\le 5\ \text{ms}$ per keystroke (Proposed) | 50k vocabulary Trie, prefix length $\ge 2$ | Qt event loop timing telemetry |
| **NFR-004** | Performance | Plain text indexing throughput (I/O + Tokenization) | $\ge 5\ \text{MB/sec}$ (Proposed Target) | NVMe SSD, UTF-8 text corpus | Batch synthetic corpus ingestion timer |
| **NFR-005** | Performance | Cold index deserialization time (100k entries) | $\le 3.0\ \text{seconds}$ (Proposed Target) | Cold disk cache, binary format with CRC32 | Startup stopwatch logging |
| **NFR-006** | Scalability | Maximum cataloged file capacity (Document Catalog) | $\ge 250,000$ files (Proposed Target) | 64-bit address space, sequential DocIDs | Dynamic array stress test |
| **NFR-007** | Scalability | Maximum vocabulary capacity in custom Hash Table | $\ge 500,000$ distinct terms (Target) | Separate chaining, $\alpha \le 0.75$ | Vocabulary stress test |
| **NFR-008** | Memory | Idle RAM usage (empty index) | $\le 45\ \text{MB}$ (Proposed Target) | GUI initialized, zero folders loaded | Windows Task Manager / VMMap |
| **NFR-009** | Memory | Working RAM usage (100,000 indexed files corpus) | $\le 350\ \text{MB}$ (Proposed Target) | Full in-memory catalog + inverted index | Memory profiler |
| **NFR-010** | Memory | Memory leaks | Exactly 0 byte leak on shutdown | Clean application exit | Valgrind / Dr. Memory / MSVC CRT Leak Checker |
| **NFR-011** | Reliability | Scanner resilience against unreadable / locked files | 100% catch rate (zero crashes) | Traversing locked OS system folders | Automated test on locked system files |
| **NFR-012** | Usability | GUI thread responsiveness during background scan | Target smooth UI (frame latency $\le 16.6\ \text{ms}$) | Worker isolated on dedicated `QThread` | Qt frame time analyzer |
| **NFR-013** | Usability | Search syntax error feedback | Clear visual indicator $\le 100\ \text{ms}$ | Invalid query input (e.g., unclosed paren) | UI automated testing |
| **NFR-014** | Maintainability| Code modularity and standard compliance | C++17 compliant, zero warnings on `/W4` | MSVC 2022 compiler check | Strict build flag verification |
| **NFR-015** | Security | Sandbox and file modification protection | Read-only access to user files | Attempt write operations during scan | Security code audit |
| **NFR-016** | Privacy | Local data residency | Zero telemetry or network transmissions | Active network monitor | Wireshark network inspection |

---

## 5. User Scenarios

### Scenario 1: User Searches for a PDF by Filename
- **Context:** An engineering student needs to locate their course syllabus (`Data_Structures_Syllabus_2026.pdf`) buried deep inside nested university folders.
- **Actions:**
  1. Student launches Universal Smart Search Engine.
  2. Student enters `syllabus` in the search bar.
  3. Student selects the "Documents" filter tab.
- **System Behavior:**
  - Trie provides instant autocomplete: `syllabus`.
  - Inverted Index identifies matching filename tokens.
  - Document Catalog filters matches by extension `.pdf`.
  - The exact file appears at Rank 1 in under $10\ \text{ms}$ with its path, size, and date.
  - Student double-clicks the result; the PDF opens in the default OS PDF viewer.

### Scenario 2: User Searches for a Term Inside Document Content
- **Context:** User remembers writing notes about "Dijkstra Shunting Yard" inside a text or markdown file, but does not remember the filename.
- **Actions:**
  1. User enters `Dijkstra Shunting Yard` in the search box.
- **System Behavior:**
  - Query parser treats input as implicit conjunction: `Dijkstra AND Shunting AND Yard`.
  - Inverted Index retrieves posting lists for `dijkstra`, `shunting`, and `yard`.
  - Posting list intersection identifies 3 candidate markdown files containing all three tokens.
  - TF-IDF scores each candidate based on term frequency and corpus rarity.
  - Results appear in descending relevance order.
  - Selecting Rank 1 loads the text preview pane with all occurrences of "Dijkstra", "Shunting", and "Yard" highlighted in yellow.

### Scenario 3: User Searches for an Image File
- **Context:** User searches for a photo named `Campus_Sunset_Graduation.jpg`.
- **Actions:**
  1. User enters `sunset graduation` and clicks the "Images" tab.
- **System Behavior:**
  - Metadata indexing extracted filename tokens (`sunset`, `graduation`) during initial scan even though image binary content was not parsed.
  - Result list displays the image match with thumbnail reference, image dimensions, and byte size.
  - Clicking the result renders an interactive scaled image preview in the GUI preview pane.

### Scenario 4: User Executes a Multi-Word Search with Ranking
- **Context:** A developer searches an indexed C++ repository for `memory leak detection allocator`.
- **Actions:**
  1. Developer enters query `memory leak detection allocator`.
- **System Behavior:**
  - Evaluates postings for each term.
  - Calculates TF-IDF score: files containing rare terms like `allocator` combined with frequent occurrences of `leak` receive significantly higher scores.
  - Max-Heap extracts the top 50 matches.
  - Result table displays high-relevance header and source files at the top of the list.

### Scenario 5: User Uses Complex Boolean Query (`AND`, `OR`, `NOT`, Parentheses)
- **Context:** User searches for project documentation involving C++ or Python, but specifically excluding legacy Python 2 files.
- **Actions:**
  1. User enters: `(cpp OR python) AND guide NOT legacy`.
- **System Behavior:**
  - Shunting-Yard parser converts infix expression into postfix: `cpp python OR guide AND legacy NOT`.
  - Stack processes operations:
    1. Union of `cpp` and `python` postings.
    2. Intersection of result with `guide` postings.
    3. Difference subtraction of all postings containing `legacy`.
  - Cleaned candidate list is scored and displayed.
  - If user accidentally typed `(cpp OR python AND`, the parser catches the missing closing parenthesis and shows a red tooltip: *"Syntax Error: Unmatched opening parenthesis at position 1"*.

### Scenario 6: User Receives Autocomplete Suggestions
- **Context:** User starts typing a technical term `algo...`.
- **Actions:**
  1. User types `a` `l` `g` `o`.
- **System Behavior:**
  - Keystroke event queries the custom Trie with prefix `algo`.
  - Trie traverses root $\to$ 'a' $\to$ 'l' $\to$ 'g' $\to$ 'o'.
  - DFS from prefix node gathers matching complete words: `algorithm`, `algorithms`, `algorithmic`, `algol`.
  - Suggestions are ordered by occurrence frequency.
  - A dropdown menu displays the top 5 completions beneath the search box.
  - User presses `Down Arrow` + `Enter` to auto-fill `algorithm`.

### Scenario 7: User Receives Spelling Correction
- **Context:** User inadvertently mistypes `dijktra` instead of `dijkstra`.
- **Actions:**
  1. User submits query: `dijktra`.
- **System Behavior:**
  - Lookup in Inverted Index yields 0 hits.
  - Spell checker triggers: explores Trie vocabulary within Levenshtein edit distance 2.
  - Identifies `dijkstra` (distance 1: missing 's').
  - Search dashboard displays: *"Showing 0 results for 'dijktra'. Did you mean: **dijkstra**? (Click to search)"*.
  - Clicking the suggestion instantly executes the corrected query.

### Scenario 8: User Inspects File in Preview Pane
- **Context:** User locates a source file `TreeBenchmark.cpp` and wants to verify its benchmark parameters without opening an external heavy IDE.
- **Actions:**
  1. User clicks `TreeBenchmark.cpp` in the result table.
- **System Behavior:**
  - GUI sends request to `ContentExtractor` / File Previewer.
  - Content viewer loads the file into a read-only text view with line numbers.
  - Query terms are auto-scrolled into view and highlighted.
  - Bottom status bar displays: *"Encoding: UTF-8 | Lines: 420 | Size: 14.2 KB"*.

### Scenario 9: User Rebuilds Index After Modifying Files
- **Context:** User created 5 new source files and edited 2 existing files in their project directory, then returns to the search engine.
- **Actions:**
  1. User clicks the "Refresh / Incremental Sync" button.
- **System Behavior:**
  - Background worker rescans selected root folders.
  - Compares timestamps against stored `DocumentCatalog`:
    - 5 new files identified $\to$ assigned new `DocID`s, metadata stored, content extracted and indexed.
    - 2 modified files identified $\to$ old postings cleared/invalidated, updated content re-indexed.
    - Unmodified files skipped in $O(1)$ per file.
  - Incremental sync completes in $210\ \text{ms}$.
  - UI updates badge: *"Index up to date (7 changes applied)"*.

### Scenario 10: User Inspects Index Statistics
- **Context:** University instructor tests the indexing capacity and internal data structure distribution of the student project.
- **Actions:**
  1. Instructor opens "View $\to$ Index Statistics" dialog.
- **System Behavior:**
  - Telemetry modal displays:
    - Total Indexed Documents: `12,450`
    - Document Breakdown: `Documents: 4,120 | Code: 6,830 | Images: 1,200 | Videos: 150 | Audio: 150`
    - Total Vocabulary Terms (Hash Table Keys): `48,912`
    - Hash Table Metrics: `Buckets: 65,536 | Load Factor: 0.746 | Max Chain Length: 5`
    - Total Postings (Linked List Nodes): `612,400`
    - Trie Nodes: `128,450`
    - Memory Consumed: `58.4 MB`
    - On-Disk Index Size: `18.2 MB`
    - Last Scan Time: `4.8 seconds`

---

## 6. Constraints and Out of Scope

### 6.1 Strictly Out of Scope for Release 1
To ensure maximum academic quality and architectural depth without over-scoping, the following features are explicitly excluded from Release 1:
1. **Live Internet & Web Crawling:** No HTTP/HTTPS web scraping, online search engine querying, or web APIs.
2. **Cloud Storage API Integration:** No direct Google Drive, Dropbox, or OneDrive REST API syncing (only locally mounted drive folders are scanned).
3. **AI / Neural Semantic Search:** No deep-learning embeddings, vector databases, BERT models, or GPU-dependent inference engines. Ranking is purely statistical (TF-IDF).
4. **Mandatory Deep Media Understanding:** No built-in computer vision image classification, OCR, audio speech-to-text, or video frame analysis. Media files participate in search via comprehensive metadata and filename tokenization only.
5. **Real-Time Kernel Filesystem Watcher:** No Windows `ReadDirectoryChangesW` daemon running continuously in background; updates are triggered manually or on application launch.
6. **Network Distributed Indexing:** No multi-node clustering or distributed search.

### 6.2 Reserved Extension Hooks for Future Releases
- **Release 2:** Real-time Windows directory change listener daemon.
- **Release 3:** Tesseract OCR plugin for scanned PDF and image content indexing.
- **Release 4:** FFmpeg audio metadata and speech-to-text transcription engine.

---

## 7. Acceptance Criteria Checklist

A successful implementation of Release 1 must pass the following verifiable criteria:

- [ ] **AC-01:** System discovers, classifies, and indexes files across single directories, nested subdirectories, and full drive roots.
- [ ] **AC-02:** Every discovered file is cataloged in the `DocumentCatalog` with complete metadata regardless of format.
- [ ] **AC-03:** Plain text and source code files have their full content tokenized and stored in the custom Inverted Index.
- [ ] **AC-04:** Non-text files (images, audio, video, archives, binaries) are discoverable by filename and metadata attributes without extraction crashes.
- [ ] **AC-05:** Inverted Index is implemented using a custom Hash Table with separate chaining (no `std::unordered_map` for core index).
- [ ] **AC-06:** Posting lists are implemented using custom sorted singly linked lists (no `std::list` or `std::vector` for core postings).
- [ ] **AC-07:** Boolean queries supporting `AND`, `OR`, `NOT`, and parenthetical grouping evaluate with 100% logical accuracy.
- [ ] **AC-08:** Relevance ranking utilizes TF-IDF formulas and extracts Top-$K$ results via a custom Max-Heap (no `std::priority_queue`).
- [ ] **AC-09:** Search bar provides instant keystroke autocomplete powered by a custom Trie structure.
- [ ] **AC-10:** Mispelled queries offer valid "Did you mean?" suggestions using Levenshtein distance $\le 2$.
- [ ] **AC-11:** Long-running indexing tasks execute on a secondary background thread, leaving the Qt GUI responsive at 60 FPS.
- [ ] **AC-12:** The index can be saved to disk, reloaded on startup, and repaired if corrupted.
- [ ] **AC-13:** Content viewer displays text files with accurate query term highlighting.
- [ ] **AC-14:** Zero memory leaks detected upon clean application exit.
- [ ] **AC-15:** All code compiles warning-free under MSVC 2022 x64 with C++17 standard enabled.
