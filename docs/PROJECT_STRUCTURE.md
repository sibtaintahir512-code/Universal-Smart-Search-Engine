# Repository & Project Structure Specification (PROJECT_STRUCTURE)
## Project: Universal Smart Search Engine
### Release 1 — Local Universal File Search Engine
**Document Version:** 1.0.0  
**Target Platform:** Windows Desktop (C++17 / MSVC 2022 x64 / Qt 6 Widgets / CMake)  
**Academic Context:** University Data Structures & Algorithms Coursework  
**Status:** Approved for Architectural Baseline  

---

## 1. Directory Tree Layout

The project repository is structured to maintain strict separation of concerns, high compile-time isolation, and clarity for academic evaluation:

```
UniversalSmartSearchEngine/
├── CMakeLists.txt                  # Root build configuration
├── cmake/                          # CMake helper modules and toolchain configs
│   ├── CompilerWarnings.cmake      # Strict warning flags (/W4 on MSVC)
│   └── Sanitizers.cmake            # ASan / UBSan configuration
│
├── docs/                           # Authoritative system documentation
│   ├── SRS.md                      # Software Requirements Specification
│   ├── ARCHITECTURE.md             # System Architecture & Diagrams
│   ├── DSA_DESIGN.md               # Custom Data Structures & Algorithms Design
│   ├── FILE_INDEXING_ARCHITECTURE.md# Indexing Subsystem Specification
│   ├── SEARCH_ENGINE_DESIGN.md     # Search Pipeline & TF-IDF Design
│   ├── PERSISTENCE_DESIGN.md       # Binary Storage & Serialization
│   ├── GUI_DESIGN.md               # Qt 6 Widgets UI & Screen Specifications
│   ├── MODULE_INTERFACES.md        # Detailed Module Contracts & Interfaces
│   ├── TEST_PLAN.md                # Multi-tier QA & Testing Strategy
│   ├── PROJECT_STRUCTURE.md        # Directory Layout & File Organization
│   └── DEVELOPMENT_ROADMAP.md      # Phase 0 through 10 Release Plan
│
├── include/                        # Public C++ header files
│   ├── core/                       # Core common types and utilities
│   │   ├── CommonTypes.hpp         # DocID, FileCategory, ExtractionStatus enums
│   │   ├── Result.hpp              # Typed Result/Error monadic wrapper
│   │   ├── Logger.hpp              # Thread-safe logging subsystem
│   │   └── FileSystemUtils.hpp     # Path normalizers and directory helpers
│   │
│   ├── dsa/                        # Pure custom data structures (Zero STL)
│   │   ├── CustomSinglyLinkedList.hpp
│   │   ├── CustomDynamicArray.hpp
│   │   ├── CustomHashTable.hpp
│   │   ├── CustomPostingList.hpp
│   │   ├── CustomTrie.hpp
│   │   ├── CustomMaxHeap.hpp
│   │   ├── CustomStack.hpp
│   │   └── CustomQueue.hpp
│   │
│   ├── indexing/                   # File discovery and metadata catalog
│   │   ├── FileScanner.hpp
│   │   ├── FileMetadataManager.hpp
│   │   ├── DocumentCatalog.hpp
│   │   └── IndexingCoordinator.hpp
│   │
│   ├── extraction/                 # Content extraction interfaces & decoders
│   │   ├── IContentExtractor.hpp
│   │   ├── TextExtractor.hpp
│   │   ├── CodeExtractor.hpp
│   │   └── ContentExtractorRegistry.hpp
│   │
│   ├── search/                     # Tokenization, parsing, and Boolean logic
│   │   ├── TextProcessor.hpp
│   │   ├── QueryParser.hpp
│   │   ├── QueryToken.hpp
│   │   ├── BooleanSearchEngine.hpp
│   │   ├── TrieAutocomplete.hpp
│   │   └── SpellChecker.hpp
│   │
│   ├── ranking/                    # Relevance scoring and priority queue
│   │   ├── InvertedIndex.hpp
│   │   ├── RankingEngine.hpp
│   │   └── SearchResultDTO.hpp
│   │
│   ├── storage/                    # Persistence and binary serialization
│   │   ├── IndexStorage.hpp
│   │   └── BinarySerializer.hpp
│   │
│   ├── history/                    # Query search history manager
│   │   └── SearchHistory.hpp
│   │
│   └── gui/                        # Qt 6 Widgets presentation layer
│       ├── MainWindow.hpp
│       ├── SearchWidget.hpp
│       ├── ResultTableWidget.hpp
│       ├── FilePreviewWidget.hpp
│       ├── FilterPanelWidget.hpp
│       ├── StatsDialog.hpp
│       └── SearchController.hpp
│
├── src/                            # Implementation source files
│   ├── main.cpp                    # Application bootstrap and entry point
│   ├── core/
│   ├── indexing/
│   ├── extraction/
│   ├── search/
│   ├── ranking/
│   ├── storage/
│   ├── history/
│   └── gui/
│
├── tests/                          # Automated test suites
│   ├── CMakeLists.txt              # Test runner build configuration
│   ├── unit/                       # Level 1 Pure DSA unit tests
│   │   ├── Test_SinglyLinkedList.cpp
│   │   ├── Test_DynamicArray.cpp
│   │   ├── Test_HashTable.cpp
│   │   ├── Test_PostingList.cpp
│   │   ├── Test_Trie.cpp
│   │   ├── Test_MaxHeap.cpp
│   │   ├── Test_Stack.cpp
│   │   └── Test_Queue.cpp
│   │
│   ├── integration/                # Level 2 Subsystem integration tests
│   │   ├── Test_ScannerCatalog.cpp
│   │   ├── Test_ParserBoolean.cpp
│   │   ├── Test_RankingHeap.cpp
│   │   └── Test_StorageRoundtrip.cpp
│   │
│   └── benchmarks/                 # Level 4 Synthetic benchmark harness
│       └── Benchmark_Corpus100k.cpp
│
├── resources/                      # Application assets
│   ├── icons/                      # Category and action UI icons (.svg, .png)
│   ├── styles/                     # Qt StyleSheet (.qss) dark theme
│   └── stopwords/                  # Standard English stop words file (stopwords.txt)
│
└── data/                           # Runtime local data directory (.gitignored)
    ├── search.idx                  # Active binary index file
    ├── history.dat                 # User search history
    └── logs/                       # Application execution logs
```

---

## 2. Directory Responsibilities

- **`include/dsa/`:** Encapsulates the algorithmic coursework components. Contains only pure C++17 template headers and custom data structures without any dependencies on Qt or external third-party libraries.
- **`include/indexing/` & `src/indexing/`:** Contains directory crawling, metadata parsing, and `DocumentCatalog` structures.
- **`include/extraction/` & `src/extraction/`:** Decoupled format decoders. Isolates file I/O and potential external libraries (Poppler, etc.) from the core engine.
- **`include/search/` & `src/search/`:** Implements the Shunting-Yard parser, query AST, Trie prefix lookup, and Levenshtein spell checker.
- **`include/ranking/` & `src/ranking/`:** Implements TF-IDF scoring and the `InvertedIndex` orchestrator.
- **`include/storage/` & `src/storage/`:** Binary serializer and CRC32 file integrity validation.
- **`include/gui/` & `src/gui/`:** Pure Qt 6 Widgets presentation layer, isolated from core structures via `SearchController`.
- **`tests/`:** Test harnesses separated into unit tests, integration tests, and benchmarks.

---

## 3. Build System Architecture (CMake)

### 3.1 Target Decomposition
To enforce unidirectional dependencies and prevent compilation cycles:
1. `DsaLib` (Static Library): Contains `include/dsa/`. Depends only on standard C++17 runtime.
2. `SearchEngineCore` (Static Library): Contains `indexing`, `extraction`, `search`, `ranking`, `storage`, `history`. Depends on `DsaLib`.
3. `SearchEngineApp` (Executable): Contains `src/main.cpp` and `gui/`. Links `SearchEngineCore`, `DsaLib`, and `Qt6::Widgets`.
4. `UnitTestsRunner` (Executable): Compiles test files and links `SearchEngineCore` and `DsaLib`.

### 3.2 Compilation Flags (MSVC 2022)
- Language Standard: `/std:c++17`
- Warning Level: `/W4` (All meaningful warnings enabled)
- Warnings as Errors: Enabled on CI/test builds (`/WX`)
- Runtime Exception Handling: `/EHsc`
- UTF-8 Source Encoding: `/utf-8`
- Multi-Processor Compilation: `/MP`
