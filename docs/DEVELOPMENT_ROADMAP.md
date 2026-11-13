# Implementation Roadmap (DEVELOPMENT_ROADMAP)
## Project: Universal Smart Search Engine
### Release 1 — Local Universal File Search Engine
**Document Version:** 1.0.0  
**Target Platform:** Windows Desktop (C++17 / MSVC 2022 x64 / Qt 6 Widgets / CMake)  
**Academic Context:** University Data Structures & Algorithms Coursework  
**Status:** Approved for Architectural Baseline  

---

## 1. Roadmap Overview & Phased Execution Plan

The development lifecycle for Release 1 is organized into 11 strictly sequenced phases. Each phase establishes clean prerequisites for subsequent stages, ensuring that foundational custom data structures are verified before ingestion and retrieval engines are built upon them.

```
[Phase 0: Architecture & Specs (Current)]
        │
        ▼
[Phase 1: Environment & CMake Setup]
        │
        ▼
[Phase 2: Custom DSA Foundation Library] ──► (Unit Tests Level 1)
        │
        ▼
[Phase 3: Universal File Scanner & Catalog]
        │
        ▼
[Phase 4: Content Extraction Engine]
        │
        ▼
[Phase 5: Inverted Index & Tokenizer Pipeline]
        │
        ▼
[Phase 6: Boolean Query Engine & Shunting-Yard Parser]
        │
        ▼
[Phase 7: TF-IDF Ranking, Max-Heap & Smart Autocomplete]
        │
        ▼
[Phase 8: Persistence & Binary Serialization]
        │
        ▼
[Phase 9: Qt 6 Widgets GUI & Preview Docks]
        │
        ▼
[Phase 10: System Integration, Benchmarking & Final QA]
```

---

## 2. Phase-by-Phase Technical Breakdown

---

### Phase 0: Requirements, Architecture & Technical Planning
- **Status:** Complete (Current Phase).
- **Objectives:** Establish comprehensive system specifications, custom DSA models, architectural boundaries, module contracts, and testing matrices.
- **Tasks:**
  1. Author `SRS.md`, `ARCHITECTURE.md`, `DSA_DESIGN.md`.
  2. Author `FILE_INDEXING_ARCHITECTURE.md`, `SEARCH_ENGINE_DESIGN.md`, `PERSISTENCE_DESIGN.md`.
  3. Author `GUI_DESIGN.md`, `MODULE_INTERFACES.md`, `TEST_PLAN.md`, `PROJECT_STRUCTURE.md`, `DEVELOPMENT_ROADMAP.md`.
  4. Perform consistency checks across all technical documents.
- **Deliverables:** Complete 11-document documentation suite in `docs/`.
- **Completion Criteria:** User reviews and explicitly approves Phase 0 baseline.
- **Relative Complexity:** High.

---

### Phase 1: Development Environment & Toolchain Setup
- **Status:** Complete.
- **Objectives:** Configure the C++17 build environment, CMake toolchain, and repository scaffold.
- **Tasks:**
  1. Create directory tree structure per `PROJECT_STRUCTURE.md`.
  2. Author root `CMakeLists.txt` and modular build scripts.
  3. Configure MSVC 2022 x64 flags (`/std:c++17`, `/W4`, `/utf-8`).
  4. Establish unit test runner executable target.
  5. Configure `.gitignore` and static analysis rules.
- **Dependencies:** Phase 0 approval.
- **Expected Deliverables:** Compiling skeleton project with zero warnings.
- **Testing Requirements:** Smoke test verifying successful build of empty `main.cpp` and test runner.
- **Completion Criteria:** CMake generates and builds clean MSVC solution.
- **Risks:** Misconfigured Qt 6 CMake paths on Windows.
- **Relative Complexity:** Low.

---

### Phase 2: Custom DSA Foundation Library
- **Status:** Complete (Verified: 1,319 / 1,319 Unit Tests Passed).
- **Objectives:** Implement all 8 custom data structures from first principles without STL container dependencies.
- **Tasks:**
  1. Implement `CustomSinglyLinkedList<T>`.
  2. Implement `CustomDynamicArray<T>` with $1.5\times$ expansion policy.
  3. Implement `CustomHashTable<K, V>` with FNV-1a hash, bitmask indexing, separate chaining, and dynamic rehashing.
  4. Implement `CustomPostingList` with monotonic `DocID` ordering and two-pointer set math (`Intersect`, `Union`, `Difference`).
  5. Implement `CustomTrie` with 36-way pointer branching and frequency counters.
  6. Implement `CustomMaxHeap<T>` with array arithmetic, `buildHeap`, and `extractMax`.
  7. Implement `CustomStack<T>` and `CustomQueue<T>`.
- **Dependencies:** Phase 1.
- **Expected Deliverables:** Templated headers in `include/dsa/` and comprehensive unit test suite in `tests/unit/`.
- **Testing Requirements:** 100% pass rate on Level 1 unit test matrix (`UT-LL-01` through `UT-QU-01`); memory leak verification via MSVC CRT debug heap.
- **Completion Criteria:** All DSA unit tests pass with zero memory leaks.
- **Risks:** Memory leaks in node destructors; off-by-one errors in heap indexing.
- **Relative Complexity:** Critical (Academic Core).

---

### Phase 3: Universal File Scanner & Cataloging
- **Status:** Complete (Verified: 1,441 / 1,441 Tests Passed).
- **Objectives:** Build resilient directory crawling and the metadata catalog.
- **Tasks:**
  1. Implement `CustomQueue`-driven BFS in `FileScanner`.
  2. Integrate `std::filesystem` with non-throwing error codes.
  3. Implement extension classifier in `FileMetadataManager`.
  4. Implement `DocumentCatalog` with contiguous string pool and dense 32-bit `DocID` assignment.
  5. Add symlink cycle suppression and duplicate path filtering.
- **Dependencies:** Phase 2.
- **Expected Deliverables:** `FileScanner`, `FileMetadataManager`, `DocumentCatalog`.
- **Testing Requirements:** Traversal tests on deep folder hierarchies; permission denial stress test on locked system folders.
- **Completion Criteria:** Successfully scans and catalogs $> 50,000$ files without crashing.
- **Risks:** OS permission exceptions terminating scan; directory junction loops.
- **Relative Complexity:** Medium.

---

### Phase 4, Phase 4.1 & Phase 4.2: Content Extraction Engine, Hardening & Compliance
- **Status:** Complete (Verified: 1,664 / 1,664 Tests Passed).
- **Objectives:** Extract decoded plain text from supported document, source code, office, pdf, and multimedia files with strict memory bounds, Google PDFium integration, XML/ZIP security safeguards, and authoritative XLSX relationship order.
- **Tasks:**
  1. Implement `IContentExtractor` plugin interface and `ExtractionResult`.
  2. Implement `TextExtractor` supporting RFC 3629 UTF-8, UTF-8 BOM, UTF-16 surrogate pairs, and RTF.
  3. Implement `SourceCodeExtractor` for 20+ programming languages preserving structure.
  4. Implement `OfficeExtractor` with safe ZIP decompression arithmetic, pugixml without fallback, PPTX slide relationship order, and XLSX workbook relationship-driven worksheet resolution (`xl/workbook.xml` + `xl/_rels/workbook.xml.rels`).
  5. Implement `PDFExtractor` integrated with official Google PDFium engine, UTF-16 surrogate pair reconstruction, and output limits during traversal.
  6. Implement `ImageMetadataExtractor`, `MediaMetadataExtractor`, and `ArchiveMetadataExtractor`.
  7. Implement `ExtractionCache` with canonical path composite identity hashing.
  8. Implement `ExtractionManager` with budget clamping (5MB–100MB), 1GB cutoff, and cooperative cancellation.
- **Dependencies:** Phase 3.
- **Expected Deliverables:** Content extraction subsystem in `extraction/`.
- **Testing Requirements:** Ingestion tests on corrupted text files, binary files disguised as `.txt`, and 100 MB large files.
- **Completion Criteria:** Zero crashes on binary garbage; graceful degradation to `MetadataOnly`.
- **Risks:** Character encoding mismatches causing corrupted strings.
- **Relative Complexity:** Medium.

---

### Phase 5: Inverted Index & Tokenizer Pipeline
- **Status:** Complete (Verified: 13,040 / 13,040 Tests Passed).
- **Objectives:** Ingest extracted content into vocabulary hash tables and posting chains.
- **Tasks:**
  1. Implement `TextProcessor`: tokenization, lowercasing, punctuation stripping, length thresholding.
  2. Implement stop-word filter using `CustomHashTable`.
  3. Implement `InvertedIndex` orchestrator managing term-to-posting-list mappings.
  4. Implement field bitmask tagging (`FIELD_FILENAME = 0x01`, `FIELD_CONTENT = 0x02`).
  5. Integrate `CustomTrie` vocabulary ingestion.
- **Dependencies:** Phase 4.
- **Expected Deliverables:** Fully functional in-memory indexing pipeline.
- **Testing Requirements:** End-to-end tokenization tests; verify posting lists maintain strict ascending `DocID` sequence.
- **Completion Criteria:** 10,000 files indexed into memory with correct term frequencies and vocabulary mappings.
- **Risks:** Excessive memory consumption during high-load tokenization.
- **Relative Complexity:** High.

---

### Phase 6: Search & Boolean Query Processing
- **Status:** Complete (Verified: 13,644 / 13,644 Tests Passed).
- **Objectives:** Parse user queries, enforce formal operator precedence, evaluate complex Boolean expressions, and retrieve document metadata with deterministic ordering.
- **Tasks:**
  1. Implement `QueryLexer` supporting case-insensitive Boolean operators (`AND`, `OR`, `NOT`), field qualifiers (`name:`, `body:`), parenthesis grouping, RFC 3629 UTF-8 validation, and rejection of unsupported wildcards/fuzzy operators.
  2. Implement `QueryParser` with recursive-descent AST construction, formal precedence (NOT > AND > OR), implicit AND insertion, syntax error reporting, and AST bounds checking.
  3. Implement `ResultSet` backed by `CustomDynamicArray<DocID>` with linear two-pointer set operations and logarithmic binary search membership.
  4. Implement `QueryEvaluator` evaluating AST expressions against `InvertedIndex` and `DocumentCatalog` with field filtering, short-circuiting, and active document universe resolution for NOT expressions ($U \setminus S$).
  5. Implement `SearchService` orchestrating end-to-end query execution, pagination (`offset`, `limit`), and `DocumentCatalog` metadata resolution with deterministic ascending DocID ordering.
  6. Implement `SearchStatistics` and `SearchIntegrityChecker` verifying non-mutating index invariant.
- **Dependencies:** Phase 5.
- **Expected Deliverables:** Complete Boolean search subsystem in `include/search/` and `src/search/`.
- **Testing Requirements:** Logical correctness tests on single-term, AND, OR, NOT, field-restricted, nested parentheses, pagination, cancellation, and invariant integrity.
- **Completion Criteria:** 100% agreement with formal set theory truth tables across synthetic test collections.
- **Risks:** Stack overflow or recursion depth on malformed queries (mitigated via AST depth limits and iterative two-pointer set evaluations).
- **Relative Complexity:** High.

---

### Phase 7: Relevance Ranking & Smart Search
- **Status:** Complete (Verified: 14,098 / 14,098 Tests Passed).
- **Objectives:** Score candidate documents using Field-Weighted TF-IDF and BM25 with independent field weights and bounded Top-K priority selection.
- **Tasks:**
  1. Implement `RankingConfig` with algorithm mode (`DocIdOrder`, `TFIDF`, `BM25`), field weights ($W_{name}=5.0, W_{body}=1.0$), and BM25 parameters ($k_1=1.2, b=0.75$).
  2. Implement `DocumentLengthStatistics` tracking per-document token counts and running corpus averages ($avgdl_{name}, avgdl_{body}$).
  3. Implement `TFIDFScorer` with BM25-smoothed and conventional smoothed IDF, field weights, and sublinear TF scaling.
  4. Implement `BM25Scorer` with independent filename and body length normalization, term frequency saturation, and numerical safety guards.
  5. Implement `TopKSelector` via `CustomMinHeap` bounded to $K$ candidates ($O(M \log K)$ complexity) with deterministic tie-breaking (higher score first, lower DocID first on tie).
  6. Implement `RankingEngine` orchestrating AST positive query term extraction (filtering out NOT subtrees) and neutral DocID handling for pure negation queries.
  7. Implement `RankingStatistics` thread-safe telemetry and integrate ranking seamlessly into `SearchService`.
- **Dependencies:** Phase 6.
- **Expected Deliverables:** Relevance ranking subsystem in `include/ranking/` and `src/ranking/`.
- **Testing Requirements:** Field weight verification, BM25 length normalization and saturation tests, Top-$K$ heap selection tests, deterministic repeatability across runs, and benchmark tests.
- **Completion Criteria:** All 14,098 assertions passing, zero undefined behavior under UBSan.
- **Risks:** Division by zero in BM25 with empty documents (mitigated with average length safeguards).
- **Relative Complexity:** High.

---

### Phase 8: Persistence & Binary Serialization
- **Objectives:** Save and load the complete search index and query history to/from disk with tombstone compaction and reference remapping.
- **Tasks:**
  1. Implement `BinarySerializer` and `BinaryDeserializer`.
  2. Implement binary file layout: Header, String Pool, Catalog, Vocabulary, Postings (16-byte on-disk record).
  3. Implement two-pass Tombstone Compaction and Reference Remapping algorithm on save.
  4. Implement IEEE 802.3 CRC32 checksum calculation.
  5. Implement atomic two-phase commit: write to `.tmp` file and rename.
  6. Implement corruption detection and clean rebuild fallback dialog.
  7. Implement separate `SearchHistory` file storage.
- **Dependencies:** Phase 7.
- **Expected Deliverables:** `IndexStorage` and persistence subsystem in `storage/`.
- **Testing Requirements:** Round-trip serialization/deserialization verification; simulated byte corruption recovery test; verify zero dangling posting references after compaction.
- **Completion Criteria:** Complete 100k-file index restored in $< 3.0\ \text{seconds}$ with 100% CRC match.
- **Risks:** Endianness or alignment padding mismatches between compiler builds.
- **Relative Complexity:** High.

---

### Phase 9: Qt 6 Widgets Desktop GUI
- **Status:** Complete (November 2026).
- **Objectives:** Construct the modern dark/light themed desktop interface and multi-threaded worker pipeline.
- **Tasks:**
  1. Create `MainWindow` layout with top search bar, filter panel, and results table.
  2. Implement custom `Trie` autocomplete dropdown widget.
  3. Implement file preview dock with text viewer and yellow keyword highlighting.
  4. Implement image preview and media metadata views.
  5. Implement telemetry dashboard and index management dialogs.
  6. Move indexing and scanning to background `QThread` via Qt signals and slots using immutable value DTOs.
  7. Implement responsive scan cancellation via atomic token.
- **Dependencies:** Phase 8.
- **Expected Deliverables:** Native Qt 6 GUI application in `gui/`.
- **Testing Requirements:** GUI responsiveness test: target smooth 60 FPS UI responsiveness (frame time $\le 16.6\ \text{ms}$) during active background indexation; zero UI thread freezing.
- **Completion Criteria:** Smooth, flicker-free desktop UI connecting all search and preview features; 14,515 passing assertions.
- **Risks:** Cross-thread data race if GUI accesses uncommitted index structures (mitigated via SearchEngineAdapter facade and thread isolation).
- **Relative Complexity:** High.

---

### Phase 10: System Integration, Benchmarking & Final QA
- **Status:** Complete (November 2026) — Release 1 Locked.
- **Objectives:** Execute full-system integration, synthetic stress benchmarking, and memory leak certification.
- **Tasks:**
  1. Run comprehensive integration test suite (`IT-01` through `IT-08`).
  2. Run failure and edge-case test suite (`FT-01` through `FT-09`).
  3. Execute synthetic benchmark suite on 1k, 10k, and 100k file corpora across cold and warm cache states on reference hardware.
  4. Run memory profiler and MSVC CRT leak detection across full user sessions.
  5. Fix compiler warnings and ensure `/W4` clean build.
  6. Prepare user manual and academic demonstration script.
- **Dependencies:** Phase 9.
- **Expected Deliverables:** Release 1 final binaries, benchmark report, and test execution logs.
- **Testing Requirements:** Zero memory leaks, zero compiler warnings, all proposed NFR targets evaluated and verified.
- **Completion Criteria:** System passes 100% of Acceptance Criteria (`AC-01` through `AC-15`).
- **Risks:** Unforeseen performance bottlenecks on mechanical hard drives (HDDs).
- **Relative Complexity:** High.
