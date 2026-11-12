# Changelog
All notable changes to the **Universal Smart Search Engine** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.9.0] - Phase 9 Complete (November 2026)

### Added
- **Qt 6 Widgets Desktop Graphical User Interface (`SearchEngineApp`, `MainWindow`):**
  - High-performance, responsive 3-pane master-detail desktop application built with Qt 6 Widgets.
  - Three-pane flexible layout (`QSplitter`): Filter sidebar, central results and search bar, and right preview/metadata dock.
  - Application lifecycle management with graceful worker thread termination on window close.
- **SearchEngineAdapter Facade (`SearchEngineAdapter`):**
  - Thread-safe bridge connecting Qt GUI components to backend core engine (`DocumentCatalog`, `InvertedIndex`, `SearchService`, `ExtractionManager`, `IndexPersistenceManager`).
  - Read-write synchronization via `std::shared_mutex` preserving zero STL container replacements in core search/indexing.
- **Virtualized Model/View Architecture (`SearchResultModel`, `SearchResultDelegate`):**
  - High-throughput `QAbstractTableModel` implementation supporting virtualized scrolling for large candidate sets.
  - Category badges, formatted byte sizes (B, KB, MB, GB), ISO timestamps, and interactive column sorting.
  - Custom item delegate rendering category pills and translucent indigo score badges.
- **Background Worker Subsystem (`SearchWorker`, `IndexingWorker`, `PreviewWorker`, `PersistenceWorker`):**
  - Dedicated background thread isolation for all heavy operations: search AST evaluation, directory traversal, file extraction, and binary disk persistence.
  - Responsive 60 FPS main GUI thread ($\le 16.6\ \text{ms}$ frame time); zero UI freezing.
  - Thread-safe cooperative cancellation via atomic tokens.
- **Modern Search Experience (`SearchBar`):**
  - Live query syntax validation displaying instant feedback for valid ASTs and detailed error tooltips for malformed syntax.
  - Sub-5ms keystroke autocomplete suggestions powered by `CustomTrie::getTopKSuggestions()`.
- **Facet Filtering & Refinement (`FilterPanel`):**
  - Real-time client-side facet filtering by file category (Documents, Code, Images, Media, Archives, Other), file size thresholds, and date ranges.
- **Multi-Format Preview Engine (`PreviewPanel`):**
  - Stacked preview supporting Code/Text with search keyword highlighting, high-DPI Image viewer with interactive zoom/fit, and Multimedia/Archive metadata summaries.
- **Deep Ranking & Diagnostics Inspector (`MetadataPanel`):**
  - Full document property inspection and BM25 / TF-IDF score breakdown visualization.
- **Dialogs & Preference Management (`IndexManagementDialog`, `DashboardDialog`, `SettingsDialog`):**
  - Directory management with real-time BFS indexing telemetry and cancellation.
  - Telemetry dashboard with corpus, vocabulary, posting node, and DSA health metrics.
  - `QSettings`-backed preferences (ranking model, Top-K slider, theme, extraction limits).
- **Dual Theme Engine (`ThemeManager`):**
  - Modern Dark Theme (Deep Charcoal `#1E1E24`, Slate Grey `#2A2B36`, Electric Indigo `#4F46E5`, Bright Amber `#F59E0B`).
  - Clean Light Theme (Neutral `#F8FAFC`, Crisp White `#FFFFFF`, Royal Indigo `#4F46E5`).
  - Dynamic hot-reloading at runtime (`Ctrl+T`).
- **Comprehensive GUI Test Suite & Regression Verification:**
  - 4 new test suites (`Test_SearchResultModel`, `Test_SearchEngineAdapter`, `Test_ThemeManager`, `Test_SearchWorker`).
  - All 14,515 assertions passing cleanly with zero failures.

---

## [0.8.0] - Phase 8 Complete (November 2026)

### Added
- **Binary Index Persistence Subsystem (`IndexPersistenceManager`, `IndexSerializer`, `IndexDeserializer`):**
  - High-throughput binary index format (`search.idx`) with 80-byte master header, section directory table, and 16-byte aligned posting records.
  - Multi-section persistence covering Tokenizer metadata, DocumentCatalog, DocumentLengthStatistics, Vocabulary dictionary, and Postings block.
  - Memory-safe, staged deserialization protocol ensuring live search structures remain untouched until full validation succeeds.
- **IEEE 802.3 CRC-32 Data Integrity (`CRC32`):**
  - Precomputed table-driven 32-bit CRC implementation with single-shot and streaming incremental calculation (> 380 MB/s).
  - Multi-layered integrity verification protecting header, section descriptors, section payloads, and trailing whole-file checksum.
- **Atomic Two-Phase Commit Protocol:**
  - Crash-safe saving via temporary file write (`search.idx.tmp`), durable disk buffer synchronization (`fsync` / `FlushFileBuffers`), post-write validation, and atomic replacement with backup preservation (`search.idx.bak`).
- **Two-Pass Reference Remapping & Tombstone Compaction (`IndexCompactor`):**
  - Purging of soft-deleted/tombstoned documents and contiguous remapping of active DocIDs to $[1 \dots N_{active}]$.
  - Zero dangling posting node references: rewrites all inverted index postings and document length records with remapped IDs.
- **Crash Recovery & Fault Isolation (`IndexRecoveryManager`):**
  - Automated diagnosis of missing/truncated/corrupt index files, isolation of damaged files to `.corrupt`, and failover to `.bak`.
- **Comprehensive Test Coverage & Equivalence Verification:**
  - 11 dedicated Phase 8 test suites verifying CRC32 known-answers, binary I/O, integrity rejection, atomic commit, and 100% round-trip query equivalence across Boolean, TF-IDF, and BM25 search queries.
  - Total assertion count increased to 14,435 passing assertions (100% pass rate, 0 failures, UBSan verified).

---

## [0.7.0] - Phase 7 Complete (November 2026)

### Added
- **Relevance Ranking Subsystem (`RankingEngine`, `RankingConfig`, `RankingResult`):**
  - Modular, extensible relevance ranking framework supporting `DocIdOrder`, `TFIDF`, and `BM25` modes.
  - Mathematical field weighting model prioritizing filename matches ($W_{name}=5.0$) over body matches ($W_{body}=1.0$).
  - Detailed score explanation breakdown (`ScoreExplanation`, `TermExplanation`) detailing per-term TF, IDF, length normalization, and field scores.
  - Thread-safe ranking telemetry recording request counts, scoring latencies, and Top-$K$ selection times (`RankingStatistics`).
- **Mathematical Scorer Implementations (`TFIDFScorer`, `BM25Scorer`):**
  - BM25-smoothed and conventional smoothed Inverse Document Frequency formulations with numerical safety guards ($DF > N$ and $DF = 0$ handling).
  - Sublinear term frequency scaling for TF-IDF.
  - Field-weighted BM25 with independent document length normalization for filename and body tokens, saturation parameter $k_1 = 1.2$, and length penalization $b = 0.75$.
  - Zero-division guards protecting corpus with zero body lengths ($avgdl = 0$).
- **Document Length Statistics (`DocumentLengthStatistics`):**
  - Thread-safe, $O(1)$ tracking of per-document filename and body token counts.
  - Running corpus totals and dynamic average document length ($avgdl$) computation.
  - Lifecycle coordination with `InvertedIndex` for incremental accumulation, document removal, and clearing.
- **Bounded Top-K Selection (`TopKSelector`):**
  - Priority heap selection using `CustomMinHeap` achieving $O(M \log K)$ selection complexity over $M$ candidates.
  - Deterministic tie-breaking prioritizing higher score first, and smaller DocID first on score tie.
- **Boolean-Aware Query Integration:**
  - Positive query-term extractor visiting AST subtrees while strictly skipping terms under `NOT` operators.
  - Fallback to neutral DocID ordering for pure negation queries (e.g., `NOT term`).
- **SearchService End-to-End Integration:**
  - Ranking mode support, score population, rank position assignment (1-based), and score explanation delivery.
  - Paged retrieval over heap-selected top results preserving total matching count.

### Changed
- `InvertedIndex` embeds `DocumentLengthStatistics` to record token lengths during indexing and update totals on document removal.
- `SearchResultItem` extended with `relevanceScore`, `rankPosition`, and `explanation`.
- `SearchResponse` extended with `rankingMode`.
- Total test assertion count increased to 14,104 passing assertions (100% pass rate, 0 failures).

### Fixed
- `TFIDFScorer`: Corrected linear body term frequency to proper sublinear transform ($1.0 + \ln(\text{bodyTf})$).
- `RankingEngine`: Corrected double negation query traversal (`NOT NOT term`) to properly score positive query terms.
- `Test_Phase7_Benchmarks`: Increased benchmark timeout thresholds under sanitizer instrumentation.

---

## [0.6.0] - Phase 6 Complete (November 2026)

### Added
- **Query Lexical Analyzer (`QueryLexer`, `QueryToken`):**
  - Case-insensitive Boolean operator detection (`AND`, `OR`, `NOT`, `and`, `or`, `not`).
  - Field qualifier detection (`name:`, `body:`, case-insensitive `NAME:`, `BODY:`).
  - Explicit rejection of unsupported search syntax: wildcards (`*`, `?`), fuzzy (`~`), proximity (`^`).
  - Parenthesis grouping tokens and exact source character offset tracking for syntax error reporting.
  - RFC 3629 UTF-8 validation and configurable query-length / token-count limits.
- **Recursive-Descent Query Parser (`QueryParser`, `QueryNode`):**
  - Abstract Syntax Tree (AST) construction with strict operator precedence (Parentheses > NOT > AND > OR).
  - Implicit `AND` insertion between adjacent search terms and parenthesized subexpressions.
  - Comprehensive syntax error detection: empty queries, missing operands, mismatched parentheses, unexpected operators, and consecutive operators.
  - Exception-safe RAII smart pointer ownership (`std::unique_ptr<QueryNode>`) with AST depth and node-count limits.
- **Algorithmic Result Set (`ResultSet`):**
  - Pure custom DSA result set backed by `CustomDynamicArray<DocID>` (Zero STL container usage).
  - Strict ascending DocID ordering with duplicate elimination.
  - Linear $O(N + M)$ two-pointer intersection, union, and difference algorithms.
  - Logarithmic $O(\log N)$ binary search membership checks.
- **Boolean Query Evaluator (`QueryEvaluator`):**
  - Fast $O(1)$ posting list retrieval per search term via `InvertedIndex`.
  - Field-restricted occurrence filtering distinguishing filename and body terms.
  - Short-circuit optimization for empty AND intersections.
  - Strict document universe evaluation for unary NOT expressions ($U \setminus S$), resolved from active `DocumentCatalog` documents.
  - Tombstone exclusion guaranteeing deleted or inactive documents are never returned.
  - Cooperative atomic cancellation checks.
- **Search Facade Service (`SearchService`, `SearchResult`, `SearchResponse`):**
  - High-level orchestration from raw query string to validated `SearchResponse` DTO.
  - Metadata resolution via `DocumentCatalog` (filename, path, extension, category, size, modification time, content status).
  - Ascending DocID deterministic ordering (Zero premature Phase 7 relevance ranking).
  - Pagination support with offset, page limit, and preserved total matching counts.
  - Cumulative search telemetry via `SearchStatistics`.
- **Search Invariant Auditor (`SearchIntegrityChecker`):**
  - Verifies that search queries never mutate the inverted index (vocabulary size, total postings).
  - Confirms all returned DocIDs are active and strictly sorted.
- **Automated Test Suites & Benchmarks:**
  - Added unit test suites: `Test_QueryLexer.cpp`, `Test_QueryParser.cpp`, `Test_QueryPrecedence.cpp`, `Test_ResultSet.cpp`, `Test_BooleanEvaluator.cpp`, `Test_FieldSearch.cpp`, `Test_SearchPagination.cpp`, `Test_SearchCancellation.cpp`, `Test_SearchIntegrity.cpp`, `Test_SearchService.cpp`.
  - Added Phase 6 synthetic corpus benchmarks (100, 1,000, and 10,000 documents) evaluating single-term, AND, OR, NOT, nested, and field queries with sub-millisecond latencies.
  - Total integrated test assertions increased from 13,040 to **13,644 assertions (100% pass rate, 0 failures, 0 sanitizer warnings)**.

---

## [0.5.0] - Phase 5 Complete (November 2026)

### Added
- **Tokenizer Subsystem (`Tokenizer`, `Token`, `TokenizerConfig`):**
  - High-performance deterministic lexical tokenizer with `CustomDynamicArray<Token>` streaming.
  - Monotonic 0-based position tracking preserving original word positions even when filters skip tokens.
  - Punctuation delimiter handling, alphanumeric token grouping, and number preservation.
  - Configurable minimum/maximum token lengths, case folding, and stop-word filtering.
- **Text Normalizer (`TextNormalizer`):**
  - RFC 3629 standard UTF-8 validation rejecting overlong encodings, truncated sequences, and UTF-16 surrogates.
  - Safe handling of multi-byte UTF-8 sequences (Urdu, Arabic, Cyrillic, accented Latin) without character corruption.
  - ASCII case folding `[A-Z] -> [a-z]` preserving multi-byte payloads.
- **Inverted Index Engine (`InvertedIndex`):**
  - Primary vocabulary mapping backed by `CustomHashTable<std::string, CustomPostingList*>`.
  - Posting chains stored in sorted `DocID` order via 24-byte `PostingNode` records.
  - Independent tracking of filename (`nameTermFreq`) and body (`bodyTermFreq`) term frequencies.
  - Accurate document frequency ($DF(t)$) computed as distinct containing documents.
  - Vocabulary synchronization with `CustomTrie` for Latin alphanumeric terms `[a-z0-9]`, with safe non-ASCII bypass.
  - Complete document removal (`removeDocument`), empty posting list cleanup, and term deallocation.
- **Indexing Orchestration Pipeline (`IndexingPipeline`):**
  - Coordinates `DocumentCatalog`, content extraction (`ExtractionResult`), tokenization, and `InvertedIndex`.
  - Granular extraction status handling: full indexing for `Success`, truncated body for `Partial`, filename-only indexing for `MetadataOnly`, `UnsupportedFormat`, `Corrupted`, `Encrypted`, and `Failed`.
  - Safe atomic commit semantics with per-document local frequency accumulation before index insertion.
  - Idempotent document update (`updateDocument`) and removal (`removeDocument`) without stale postings.
- **Index Integrity Validator (`IndexIntegrityChecker`):**
  - Deep non-mutating audit validating DocID bounds, ascending posting order, non-zero frequencies, field bitmask consistency, document frequency agreement, and Trie synchronization.
- **Quality Assurance & Benchmarks:**
  - Added unit test suites: `Test_TextNormalizer.cpp`, `Test_Tokenizer.cpp`, `Test_InvertedIndex.cpp`, `Test_IndexingPipeline.cpp`, `Test_IndexIntegrity.cpp`.
  - Added synthetic corpus benchmarks: 100, 1,000, and 10,000 documents reaching $>20,000$ docs/sec ($>1.5\text{M}$ tokens/sec).
  - Total integrated test assertions increased to **13,040 assertions (100% pass rate, 0 failures, 0 sanitizer warnings)**.

---

## [0.4.2] - Phase 4.2 Complete (November 2026)

### Added
- **Production PDF Extraction Engine (Google PDFium):**
  - Integrated official Google PDFium C library (Chromium engine) for Windows x64 and macOS ARM64.
  - Page-by-page bounded text extraction, reading order preservation, and page count parsing.
  - Full UTF-16LE surrogate pair decoding into standard RFC 3629 UTF-8.
  - Dedicated password encryption detection (`FPDF_ERR_PASSWORD` $\to$ `Encrypted`).
  - Fallback parser preserves 100% backward compatibility for legacy non-standard test streams.
- **Office XML Security Hardening (`OfficeExtractor`):**
  - Completely eliminated unsafe regex tag-stripping fallback in `stripXmlAndDecode`.
  - Malformed XML documents immediately return `ExtractionStatus::Corrupted`.
  - Validated external entity resolution (XXE) and DTD fetching remain disabled in `pugixml`.
- **ZIP Decompression Security Hardening:**
  - Replaced integer division in compression ratio check with overflow-safe multiplication.
  - Explicitly rejected zero-compressed size entries declaring non-zero uncompressed size.
  - Pre-allocation size validation enforcing entry and cumulative memory bounds before buffer resizing.
- **XLSX Relationship-Based Worksheet Order:**
  - Resolved worksheet ordering by parsing `xl/workbook.xml` manifest and `xl/_rels/workbook.xml.rels`.
  - Preserves author-intended presentation order instead of arbitrary filename sorting.
  - Added support for inline strings (`<is><t>`) and empty cells.
- **Output Limit Enforcement:**
  - Enforced `maxBytes` output bounding during node and page traversal across PDF, DOCX, PPTX, and XLSX.
- **Testing & Quality Assurance:**
  - Added `tests/unit/Test_Phase42_Compliance.cpp` with 37 new targeted compliance assertions.
  - Total integrated test suite increased to 1,664 assertions (100% pass rate, 0 failures, 0 sanitizer errors).
- **Documentation:**
  - `docs/PHASE_4_2_INITIAL_AUDIT.md`: Pre-implementation defect audit and PDF library comparison.
  - `docs/PHASE_4_2_COMPLIANCE_REPORT.md`: Final Quality Assurance and Compliance Report.

---

## [0.4.1] - Phase 4.1 Complete (November 2026)

### Added
- **Hardened PDF Extraction Subsystem (`PDFDocumentParser`):**
  - Chunked 64 KB stream reader eliminating monolithic file memory allocations.
  - Linear object scanning fallback when XRef table is corrupted or missing.
  - Font `/ToUnicode` CMap parser mapping custom glyph identifiers to valid UTF-8.
  - Multi-stream and kerning array operator handling (`TJ`, `Tj`, `'`, `"`, `Td`, `T*`).
  - Strict decompression bomb limits (10 MB per stream, 50 MB total ceiling, 100:1 ratio limit).
- **Hardened Office Extraction Subsystem (`OfficeExtractor`):**
  - Integrated `miniz` 3.0.2 for ZIP64 support, in-memory entry streaming, and 10,000 entry ceiling.
  - Integrated `pugixml` 1.14 DOM parsing with external entity resolution disabled.
  - Corrected PPTX presentation slide ordering by parsing `ppt/presentation.xml` and relationships.
  - Implemented XLSX cell value resolution for shared strings (`xl/sharedStrings.xml`) and inline strings (`<is><t>`).
  - Multi-entry ZIP bomb defense (50 MB per entry, 100 MB total package ceiling, 100:1 ratio).
- **Text & Encoding Hardening (`TextExtractor`):**
  - Strict RFC 3629 UTF-8 validation rejecting overlong sequences (`0xC0, 0x80`) and surrogate halves.
  - UTF-16 surrogate pair assembly for characters outside the Basic Multilingual Plane ($U+10000 \dots U+10FFFF$).
  - Safe RTF control word stripping and text extraction.
- **Cache Canonicalization Hardening (`ExtractionCache`):**
  - Consistent canonical path indexing in `put()` and `get()` resolving relative path cache misses.
- **Testing & Quality Assurance:**
  - Added `Test_Phase41_Hardening.cpp` with 53 adversarial and hardening assertions.
  - Total test suite now checks 1,627 assertions with 100% pass rate under native build and UndefinedBehaviorSanitizer.
- **Documentation:**
  - `docs/PHASE_4_1_INITIAL_AUDIT.md`: Complete defect and security risk audit.
  - `docs/PHASE_4_1_HARDENING_REPORT.md`: Comprehensive engineering hardening report.

---

## [0.4.0] - Phase 4 Complete (November 2026)

### Added
- **Content & Metadata Extraction Subsystem:**
  - `include/extraction/ExtractionStatus.hpp`: Granular extraction status codes (`Success`, `Partial`, `MetadataOnly`, `UnsupportedFormat`, `Corrupted`, `Encrypted`, `PermissionDenied`, `FileTooLarge`, `Cancelled`, `Failed`).
  - `include/extraction/ExtractionResult.hpp` & `src/extraction/ExtractionResult.cpp`: Unified extraction result value type tracking docId, path, text, byte counts, duration, truncation flag, metadata summary, and error message.
  - `include/extraction/IContentExtractor.hpp`: Polymorphic interface for all content and metadata extractors.
  - `include/extraction/TextExtractor.hpp` & `src/extraction/TextExtractor.cpp`: Streaming text parser with UTF-8 BOM stripping, UTF-16 LE/BE conversion, and 64KB chunk processing.
  - `include/extraction/SourceCodeExtractor.hpp` & `src/extraction/SourceCodeExtractor.cpp`: Source code parser for 20+ languages preserving indentation and structure.
  - `include/extraction/OfficeExtractor.hpp` & `src/extraction/OfficeExtractor.cpp`: Direct ZIP container traversal and zlib raw deflate decompression for DOCX (`document.xml`), PPTX (`slide*.xml`), and XLSX (`sharedStrings.xml`) with anti-zip-bomb limits.
  - `include/extraction/PDFExtractor.hpp` & `src/extraction/PDFExtractor.cpp`: Stream text extraction (`BT ... ET`, `Tj`, `TJ`), FlateDecode zlib decompression, password encryption detection, and page count parsing.
  - `include/extraction/ImageMetadataExtractor.hpp` & `src/extraction/ImageMetadataExtractor.cpp`: 32KB binary header inspection for PNG, JPEG, GIF, BMP, WEBP, and SVG with external entity resolution prevention.
  - `include/extraction/MediaMetadataExtractor.hpp` & `src/extraction/MediaMetadataExtractor.cpp`: 64KB container header parsing for WAV, MP4, MOV, FLAC, and MP3 (ID3v2 tags and MPEG sync).
  - `include/extraction/ArchiveMetadataExtractor.hpp` & `src/extraction/ArchiveMetadataExtractor.cpp`: Non-extracting archive container inspector for ZIP, 7Z, RAR, TAR, and GZ.
  - `include/extraction/ExtractionCache.hpp` & `src/extraction/ExtractionCache.cpp`: Thread-safe caching layer backed by Phase 2 `CustomHashTable` keyed on composite file identity.
  - `include/extraction/ExtractionManager.hpp` & `src/extraction/ExtractionManager.cpp`: Central coordinator managing extractor registration, budget clamping (5MB–100MB), 1GB cutoff, cache lookups, and cooperative cancellation.
- **Testing & Quality Assurance:**
  - 9 dedicated Phase 4 test suites verifying all extractors, edge cases, corrupted/encrypted inputs, and caching behavior.
  - 1,574 total assertions passed (133 new in Phase 4) with 0 failures under native build and UndefinedBehaviorSanitizer (`-fsanitize=undefined`).
- **Documentation:**
  - `docs/CONTENT_EXTRACTION_IMPLEMENTATION_REPORT.md`: Detailed engineering report and format support matrix for Phase 4.

---

## [0.3.0] - Phase 3 Complete (November 2026)

### Added
- **Universal File Discovery & Metadata Subsystem:**
  - `include/indexing/FileMetadataManager.hpp` & `src/indexing/FileMetadataManager.cpp`: Non-throwing metadata extraction, classification into 8 categories (Documents, Images, Videos, Audio, SourceCode, Archives, Executables, Other), MIME type detection, and capability flags.
  - `include/indexing/DocumentCatalog.hpp` & `src/indexing/DocumentCatalog.cpp`: Thread-safe document registry with persistent path identity, dense 32-bit monotonic runtime `DocID` assignment, compact contiguous string pool, soft-tombstoning, and zero STL container core storage.
  - `include/indexing/FileChangeDetector.hpp` & `src/indexing/FileChangeDetector.cpp`: Fast $O(1)$ attribute comparison (New, Modified, Unchanged, Missing) and safe post-scan reconciliation.
  - `include/indexing/ScanStatistics.hpp` & `src/indexing/ScanStatistics.cpp`: Comprehensive telemetry tracker (discovered, scanned, cataloged, errors, elapsed time, throughput).
  - `include/indexing/FileScanner.hpp` & `src/indexing/FileScanner.cpp`: Resilient iterative BFS directory crawler driven by Phase 2 `CustomQueue`, with ancestor root deduplication, symlink loop suppression, and non-throwing error handling.
  - `include/indexing/ScanWorker.hpp` & `src/indexing/ScanWorker.cpp`: Asynchronous multithreaded scanning worker decoupling crawler from UI, with cooperative cancellation and safe RAII shutdown.
- **Testing & Quality Assurance:**
  - Independent unit and integration test suites: `Test_FileMetadataManager.cpp`, `Test_DocumentCatalog.cpp`, `Test_FileChangeDetector.cpp`, `Test_ScanStatistics.cpp`, `Test_FileScanner.cpp`, `Test_ScanWorker.cpp`.
  - 1,441 assertions passed with 0 failures under native build and UndefinedBehaviorSanitizer (`-fsanitize=undefined`).
- **Documentation:**
  - `docs/FILE_SCANNER_IMPLEMENTATION_REPORT.md`: Comprehensive technical report for Phase 3.

---

## [0.2.0] - Phase 2 Complete (November 2026)

### Added
- **Custom Data Structures Library (`DsaLib`):**
  - `CustomDynamicArray<T>`: Contiguous heap buffer, 1.5x geometric expansion, placement-new, boundary-checked `at()`.
  - `CustomSinglyLinkedList<T>`: Node-based list with head/tail pointers, $O(1)$ push/pop, multi-element `remove_if`, iterative non-recursive `clear()`.
  - `CustomHashTable<K, V>`: Separate chaining hash table, 64-bit FNV-1a, power-of-two bitmask indexing, dynamic rehashing at $\alpha > 0.75$, raw multi-byte UTF-8 key support.
  - `CustomPostingList`: Ordered chain of verified 24-byte `PostingNode` records, monotonic $O(1)$ append, sorted insertion fallback, linear $O(L_1 + L_2)$ intersect/union/difference set operations.
  - `CustomTrie`: 36-way pointer branching (`a-z`, `0-9`), `searchFrequency` tracking, DFS Top-$K$ suggestion extraction, non-ASCII fallback, iterative deletion.
  - `CustomMaxHeap<T>` & `CustomMinHeap<T>`: Binary heaps backed by dynamic array, array arithmetic, Floyd's $O(N)$ linear `buildHeap`, $O(\log K)$ `replaceTop` for Strategy B Bounded Top-$K$ streaming, deterministic `HeapNode` tie-breaking.
  - `CustomStack<T>`: Contiguous LIFO stack for Dijkstra's Shunting-Yard Boolean parsing.
  - `CustomQueue<T>`: Node-based FIFO queue for BFS directory discovery.
- **Exhaustive Unit Tests:**
  - 9 test suites in `tests/unit/` verifying 1,319 assertions with 0 failures under UBSan.

---

## [0.1.0] - Phase 1 Complete (November 2026)

### Added
- **Project Scaffolding:**
  - Standardized directory layout per `docs/PROJECT_STRUCTURE.md` (`include/`, `src/`, `tests/`, `cmake/`, `resources/`, `data/`).
  - Core header `include/core/CommonTypes.hpp` defining `DocID`, `FileCategory`, `ExtractionStatus`, `FileMetadata`, and the verified 24-byte `PostingNode` structure.
  - Compile-time assertion `static_assert(sizeof(PostingNode) == 24)` on 64-bit architectures.
  - English stop-words asset `resources/stopwords/stopwords.txt`.
- **CMake Build System:**
  - Root `CMakeLists.txt` enforcing standard C++17, Qt 6 Widgets integration, and MSVC multi-processor `/MP` flags.
  - `cmake/CompilerWarnings.cmake` with strict MSVC `/W4`, `/permissive-`, and `/utf-8` options.
  - `cmake/Sanitizers.cmake` with AddressSanitizer and UndefinedBehaviorSanitizer configurations.
- **Target Definitions:**
  - `DsaLib` (Static Library): Scaffold for custom data structures (`src/dsa/DsaLibPlaceholder.cpp`).
  - `SearchEngineCore` (Static Library): Core business and retrieval logic (`src/core/CommonTypes.cpp`).
  - `SearchEngineApp` (Executable): Minimal Qt 6 desktop application startup shell (`src/main.cpp`).
  - `UnitTestsRunner` (Executable): Sanity test runner (`tests/unit/Test_Main.cpp`).
- **Documentation:**
  - `BUILD_INSTRUCTIONS.md`: Step-by-step Windows 10/11 x64 toolchain installation and build instructions.
  - `DEVELOPMENT_STATUS.md`: Phase-by-phase tracker and environment audit findings.
  - `.gitignore`: Comprehensive ignore rules for CMake, MSVC, Qt, and local data files.

---

## [0.0.1] - Phase 0 Complete (November 2026)

### Added
- Complete foundational technical documentation suite in `docs/`:
  - `SRS.md`: Software Requirements Specification (28 FRs, NFRs, 10 user scenarios).
  - `ARCHITECTURE.md`: 7-layer architecture, 18 modules, 7 Mermaid diagrams, 12 design decisions.
  - `DSA_DESIGN.md`: Specifications for 8 custom data structures (Zero STL for core engine).
  - `FILE_INDEXING_ARCHITECTURE.md`: Universal file discovery and metadata cataloging.
  - `SEARCH_ENGINE_DESIGN.md`: 16-stage retrieval pipeline and TF-IDF scoring.
  - `PERSISTENCE_DESIGN.md`: Binary file format (`search.idx`) and CRC32 integrity check.
  - `GUI_DESIGN.md`: Qt 6 Widgets UI design for 10 required desktop screens.
  - `MODULE_INTERFACES.md`: Exact C++ abstract contracts and ownership rules.
  - `TEST_PLAN.md`: Multi-tier QA test plan and synthetic benchmarking protocol.
  - `PROJECT_STRUCTURE.md`: Repository layout and target decomposition.
  - `DEVELOPMENT_ROADMAP.md`: Sequenced 11-phase implementation plan.
