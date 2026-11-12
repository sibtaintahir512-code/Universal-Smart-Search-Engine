# UNIVERSAL SMART SEARCH ENGINE — PHASE 9 PREFLIGHT REPORT
## QT 6 DESKTOP GRAPHICAL USER INTERFACE & BACKGROUND WORKER ARCHITECTURE

**Project:** Universal Smart Search Engine  
**Release:** 1  
**Phase:** 9 — Qt 6 Desktop GUI, Search Experience, File Preview & Background Workers  
**Author:** Sibtain Abbas <sibtaintahir512@gmail.com>  
**Target Platform:** Windows 10/11 x64 (MSVC 2022 / C++17 / Qt 6 Widgets)  
**Host Environment:** macOS ARM64 (Apple Clang 17.0.0 / Qt 6.10.1 / CMake 3.30.5)  
**Date:** November 12, 2026  
**Status:** Approved for Implementation  

---

## 1. Executive Summary & Objective

Phase 9 transforms the production-grade C++17 search engine backend developed across Phases 1 through 8 into an intuitive, high-performance, and responsive desktop search application using **Qt 6 Widgets**. 

The backend comprises:
1. **Phase 2 Custom DSA Library:** Zero-STL containers (`CustomDynamicArray`, `CustomSinglyLinkedList`, `CustomHashTable`, `CustomPostingList`, `CustomTrie`, `CustomMaxHeap`, `CustomMinHeap`, `CustomStack`, `CustomQueue`).
2. **Phase 3 File Scanner & Document Catalog:** Multi-threaded BFS crawler and dense monotonic DocID catalog.
3. **Phases 4, 4.1 & 4.2 Content Extraction Engine:** Modular extraction across text, code, Office, PDF (PDFium), images, multimedia, and archives.
4. **Phase 5 Inverted Index & Tokenizer:** Thread-safe vocabulary hash table, Trie prefix indexing, and sorted posting lists.
5. **Phase 6 Boolean Query Processing:** Shunting-yard parser, AST evaluation, and field filtering.
6. **Phase 7 Relevance Ranking Engine:** BM25 and TF-IDF scoring with field weighting and top-K selection.
7. **Phase 8 Binary Persistence Subsystem:** Atomic binary serialization (`search.idx`), CRC-32 integrity validation, and compaction.

The desktop GUI must expose this backend through clean, decoupled interfaces while preserving strict architectural boundaries:
- **Zero STL Container Replacements** in the core algorithmic backend.
- **Strict Decoupling:** GUI classes (`QMainWindow`, `QWidget`, `QAbstractTableModel`) must never duplicate search, indexing, or ranking algorithms.
- **Non-Blocking GUI Thread:** The main GUI thread (handling rendering and input events) must strictly execute in $\le 16.6\ \text{ms}$ (smooth 60 FPS). All expensive tasks (disk crawling, file parsing, tokenization, AST evaluation, ranking, binary persistence, preview loading) are delegated to dedicated background worker threads (`QThread` / worker objects) synchronized via Qt's thread-safe signal/slot mechanism and atomic cancellation tokens.

---

## 2. Toolchain, Build System & Qt 6 Integration

### 2.1 Compiler and CMake Configuration
- **Standard:** Strict C++17 (`set(CMAKE_CXX_STANDARD 17)`, `CMAKE_CXX_EXTENSIONS OFF`).
- **Qt Automation:** `CMAKE_AUTOMOC ON`, `CMAKE_AUTORCC ON`, `CMAKE_AUTOUIC ON`.
- **Qt Modules:** `Qt6::Widgets`, `Qt6::Core`, `Qt6::Gui`.
- **Target `SearchEngineApp`:** Links `SearchEngineCore`, `DsaLib`, and Qt 6 modules. Post-build rules handle PDFium DLL copying on Windows and RPATH setup on macOS.

### 2.2 Compilation Matrix
| Platform | Compiler | Qt Version | Verification Status |
| :--- | :--- | :--- | :--- |
| **macOS ARM64 (Dev Host)** | Apple Clang 17.0.0 | Qt 6.10.1 | **VERIFIED** |
| **Windows 10/11 x64 (Target)** | MSVC 2022 v143 | Qt 6.2+ | **NOT VERIFIED (Host is macOS)** |

*Note on Compliance:* As established in previous phases, cross-platform source compliance is maintained using standard C++17 filesystem, fixed-width integer types (`<cstdint>`), and portable Qt 6 abstractions. Windows-specific tests are labeled NOT VERIFIED until verified in a native Windows MSVC CI runner.

---

## 3. Architecture & Subsystem Decomposition

The Phase 9 GUI subsystem is organized into 5 primary architectural layers:

```
┌────────────────────────────────────────────────────────────────────────┐
│                        SearchEngineApp (GUI)                           │
│                                                                        │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                    MainWindow & UI Components                    │  │
│  │  - SearchBar & Autocomplete Popup (CustomTrie-backed)            │  │
│  │  - FilterPanel (Category, File Size, Date Range)                 │  │
│  │  - SearchResultsView & SearchResultDelegate                      │  │
│  │  - PreviewPanel (Code/Text, Images, PDF text, Media/Archive)     │  │
│  │  - MetadataPanel (File properties, DocID, Ranking explanations)  │  │
│  │  - StatusBar & Index Telemetry                                   │  │
│  │  - Dialogs: IndexManagementDialog, DashboardDialog, Settings     │  │
│  └──────────────────────────────────┬───────────────────────────────┘  │
│                                     │ Qt Signals / Slots (Main Thread) │
│  ┌──────────────────────────────────▼───────────────────────────────┐  │
│  │                        Qt Model/View Layer                       │  │
│  │  - SearchResultModel (QAbstractTableModel - virtual scrolling)    │  │
│  └──────────────────────────────────┬───────────────────────────────┘  │
│                                     │ Queued Signals / Slots           │
│  ┌──────────────────────────────────▼───────────────────────────────┐  │
│  │                     Background Worker Subsystem                  │  │
│  │  - SearchWorker (Off-thread SearchService execution)             │  │
│  │  - IndexingWorker (Off-thread FileScanner + IndexingPipeline)     │  │
│  │  - PreviewWorker (Off-thread ExtractionManager snippet loading)  │  │
│  │  - PersistenceWorker (Off-thread Save / Load / Compact)          │  │
│  └──────────────────────────────────┬───────────────────────────────┘  │
│                                     │ Direct C++ Thread-Safe Calls     │
│  ┌──────────────────────────────────▼───────────────────────────────┐  │
│  │                     SearchEngineAdapter Facade                   │  │
│  │  - Holds instances: DocumentCatalog, InvertedIndex,              │  │
│  │    SearchService, ExtractionManager, IndexPersistenceManager     │  │
│  └──────────────────────────────────┬───────────────────────────────┘  │
└─────────────────────────────────────┼──────────────────────────────────┘
                                      │ Backend C++17 & Custom DSA
┌─────────────────────────────────────▼──────────────────────────────────┐
│                         SearchEngineCore & DsaLib                      │
│  - DocumentCatalog, InvertedIndex, Tokenizer                           │
│  - SearchService, QueryParser, QueryEvaluator                          │
│  - RankingEngine (BM25, TF-IDF, Top-K)                                 │
│  - ExtractionManager (PDFium, PugiXML, Miniz, Metadata)                │
│  - IndexPersistenceManager (Binary Serialization, CRC-32)              │
│  - CustomDynamicArray, CustomHashTable, CustomTrie, CustomPostingList   │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Component Design Specifications

### 4.1 SearchEngineAdapter Facade
- **Header:** `include/gui/adapters/SearchEngineAdapter.hpp`
- **Source:** `src/gui/adapters/SearchEngineAdapter.cpp`
- **Responsibility:** Central owner and lifecycle coordinator of the core backend structures (`DocumentCatalog`, `InvertedIndex`, `SearchService`, `ExtractionManager`, `IndexPersistenceManager`).
- **Thread Safety:** Manages synchronization and provides thread-safe accessors for worker threads.
- **Prefix Autocomplete:** Bridges `CustomTrie::getTopKSuggestions()` to provide sub-5ms keystroke autocomplete options without querying the inverted index or parsing queries.

### 4.2 Background Worker Subsystem (`include/gui/workers/`, `src/gui/workers/`)
1. **`SearchWorker`:**
   - Moves to dedicated worker `QThread`.
   - Accepts search parameters (query string, offset, limit, ranking mode, category filters).
   - Holds an `std::atomic<bool> m_cancelToken` passed to `SearchService::search()`.
   - Emits `searchCompleted(SearchResponse response)` or `searchFailed(QString error)`.
   - Cooperatively aborts prior in-flight searches when a new query is submitted.
2. **`IndexingWorker`:**
   - Coordinates multi-folder scanning via `FileScanner` and indexing via `IndexingPipeline`.
   - Emits `indexingProgress(uint64_t discovered, uint64_t indexed, QString currentPath)`.
   - Emits `indexingCompleted(IndexingStatistics stats)` or `indexingCancelled()`.
   - Supports cooperative cancellation via `std::atomic<bool> m_cancelToken`.
3. **`PreviewWorker`:**
   - Safely reads and formats file previews off-thread (e.g., text snippets, image thumbnails, PDF page text, media tags).
   - Bounds memory usage to prevent GUI stuttering on multi-gigabyte files.
   - Emits `previewLoaded(DocID docId, PreviewData data)`.
4. **`PersistenceWorker`:**
   - Executes `saveIndex()`, `loadIndex()`, `compactIndex()`, or `recoverIndex()` on a secondary thread.
   - Emits `persistenceProgress(QString stage)` and `persistenceCompleted(PersistenceResult result)`.

### 4.3 Model/View Layer (`include/gui/models/`, `src/gui/models/`)
- **`SearchResultModel`:** Subclasses `QAbstractTableModel`.
  - Backed by `dsa::CustomDynamicArray<search::SearchResultItem>`.
  - Column 0: Category / Icon
  - Column 1: Filename
  - Column 2: Relevance Score (with normalized visual score)
  - Column 3: File Size (formatted: B, KB, MB, GB)
  - Column 4: Last Modified Date
  - Column 5: Path
  - Supports efficient updates without re-allocating model data unnecessarily.
- **`SearchResultDelegate`:** Custom item delegate rendering score badges and category pills with crisp high-DPI rendering.

### 4.4 UI Components & Panels (`include/gui/widgets/`, `src/gui/widgets/`)
1. **`SearchBar`:**
   - Modern search input with magnifying glass icon, clear button, and shortcut (`Ctrl+K` / `Ctrl+F`).
   - Autocomplete dropdown powered by `CustomTrie::getTopKSuggestions()`.
   - Live query validation indicator (green badge on valid AST, red alert on unbalanced parentheses or syntax errors).
2. **`FilterPanel`:**
   - Category checkboxes: All, Documents, Code, Images, Media, Archives.
   - Size filter: Any, <1 MB, 1–10 MB, 10–100 MB, >100 MB.
   - Date modified filter: Any time, Past 24 hours, Past week, Past month, Past year.
3. **`SearchResultsView`:**
   - High-performance `QTableView` configured with single row selection, virtualized scrolling, and alternating row colors.
   - Double-click triggers `QDesktopServices::openUrl()` to launch file in OS default application.
   - Single-click signals the `PreviewPanel` and `MetadataPanel`.
4. **`PreviewPanel`:**
   - Stacked widget switching based on file category:
     - **Text/Code Viewer:** `QPlainTextEdit` with line numbers, monospace font, and yellow search keyword highlighting.
     - **Image Viewer:** Scaled `QLabel` displaying high-quality `QPixmap` with zoom controls.
     - **PDF Viewer:** Extracted text display with page counts and metadata.
     - **Media / Archive Viewer:** Formatted property sheets with metadata tags.
5. **`MetadataPanel`:**
   - Detailed inspector showing document ID, storage path, creation/modification timestamps, file size, token count, and BM25/TF-IDF score breakdown.
6. **`StatusBar`:**
   - Telemetry strip displaying total active documents, indexed vocabulary count, search query latency (ms), match count, and background worker state.
7. **`ThemeManager`:**
   - Complete stylesheet engine providing:
     - **Dark Theme:** Deep Charcoal `#1E1E24`, Slate Grey `#2A2B36`, Electric Indigo `#4F46E5`, Bright Amber `#F59E0B`.
     - **Light Theme:** Modern clean neutral palette with high contrast and subtle borders.
     - Dynamic hot-reloading of QSS stylesheets.
8. **Dialogs:**
   - **`IndexManagementDialog`:** Folder list with Add/Remove, Rebuild / Incremental Sync buttons, real-time progress bar, and cancellation.
   - **`DashboardDialog`:** Telemetry cards for corpus size, inverted index terms, posting nodes, memory footprint, and hash table load factors.
   - **`SettingsDialog`:** `QSettings`-persisted preferences (default ranking algorithm BM25/TF-IDF/DocID, Top-K slider, snippet length, theme toggle).

---

## 5. Implementation Roadmap & Verification Plan

1. **Step 1:** Create `SearchEngineAdapter` and `SearchResultModel`.
2. **Step 2:** Implement background worker classes (`SearchWorker`, `IndexingWorker`, `PreviewWorker`, `PersistenceWorker`).
3. **Step 3:** Implement styling system (`ThemeManager`, QSS stylesheets).
4. **Step 4:** Implement UI widgets (`SearchBar`, `FilterPanel`, `SearchResultsView`, `SearchResultDelegate`, `PreviewPanel`, `MetadataPanel`).
5. **Step 5:** Implement dialogs (`IndexManagementDialog`, `DashboardDialog`, `SettingsDialog`).
6. **Step 6:** Implement `MainWindow` orchestrating all components.
7. **Step 7:** Connect `src/main.cpp` as application entry point.
8. **Step 8:** Update `CMakeLists.txt` and `tests/CMakeLists.txt` with GUI targets and automated GUI/Adapter unit tests.
9. **Step 9:** Execute full unit test suite and verify 100% passing tests.
10. **Step 10:** Document full implementation report `docs/PHASE_9_GUI_IMPLEMENTATION_REPORT.md` and enforce Phase 10 Lock.

---
*Preflight approved. Ready to proceed with Phase 9 implementation.*
