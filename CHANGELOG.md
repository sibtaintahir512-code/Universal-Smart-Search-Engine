# Changelog
All notable changes to the **Universal Smart Search Engine** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
