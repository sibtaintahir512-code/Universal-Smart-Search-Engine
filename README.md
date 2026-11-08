# Universal Smart Search Engine
### Release 1 — Local Universal File Search Engine

[![C++17](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![Compiler](https://img.shields.io/badge/Compiler-MSVC%202022%20x64-brightgreen.svg)](https://visualstudio.microsoft.com/)
[![Framework](https://img.shields.io/badge/UI-Qt%206%20Widgets-green.svg)](https://www.qt.io/)
[![Build](https://img.shields.io/badge/Build-CMake%203.20%2B-orange.svg)](https://cmake.org/)
[![Status](https://img.shields.io/badge/Phase-Phase%204.2%20Complete-success.svg)](DEVELOPMENT_STATUS.md)

A high-performance native desktop file search engine built from the ground up in modern **C++17** and **Qt 6 Widgets**. 

Engineered for a university Data Structures and Algorithms coursework project, the application replaces black-box search databases with **pure, custom-built data structures** (Custom Hash Table with separate chaining, sorted Posting Lists, Prefix Trie, and Max/Min-Heap priority queues).

---

## Key Capabilities (Architectural Design)

- **Universal File Discovery:** Scans full drive roots and nested directories using Breadth-First Search (BFS).
- **Dual-Tier Indexing:**
  - *Metadata Index (100% of corpus):* Every file (documents, source code, images, audio, video, archives, system binaries) is indexed by name, extension, path, size, and dates. Non-text files are fully discoverable.
  - *Content Index:* Extracts and indexes body text from plain text and source code, with extensible interfaces for PDF and Office documents.
- **Complex Boolean Search:** Evaluates `AND`, `OR`, `NOT`, and parenthetical expressions using Dijkstra's Shunting-Yard parser.
- **Statistical Relevance Ranking:** Computes field-weighted TF-IDF scores with independent filename and body term frequencies.
- **Top-$K$ Heap Selection:** Extracts top search results via bounded Min-Heap / Max-Heap priority queues.
- **Intelligent Search Assistance:** Sub-5ms keystroke autocomplete powered by a 36-way custom Trie with non-ASCII fallback, and Levenshtein spell correction.
- **Persistent Storage:** Compact binary file format (`search.idx`) with CRC32 integrity validation and atomic save protocols.
- **Modern Dark-Themed GUI:** Non-blocking multi-threaded Qt 6 interface with file preview docks and keyword highlighting.

---

## Fixed Technology Stack

- **Operating System:** Windows 10/11 x64
- **Language / Standard:** ISO C++17 (`/std:c++17 /permissive- /utf-8`)
- **Compiler:** Microsoft Visual C++ 2022 (MSVC v143 x64)
- **GUI Framework:** Qt 6.2+ Widgets (MSVC 2022 64-bit)
- **Build System:** CMake 3.20+
- **Editor:** Visual Studio Code with CMake Tools and C/C++ Extension Pack

---

## Repository Structure

```
├── CMakeLists.txt                  # Root build configuration
├── cmake/                          # CMake warning and sanitizer modules
├── docs/                           # Authoritative system documentation (11 files)
├── include/                        # Public C++ headers
│   ├── core/                       # Core data types and enumerations
│   ├── dsa/                        # Custom data structures (Zero STL)
│   ├── indexing/                   # Crawler and DocumentCatalog
│   ├── extraction/                 # File format decoders
│   ├── search/                     # Tokenizer, QueryParser, BooleanEngine
│   ├── ranking/                    # InvertedIndex, TF-IDF, MaxHeap
│   ├── storage/                    # Binary serialization & persistence
│   ├── history/                    # Search history manager
│   └── gui/                        # Presentation layer
├── src/                            # Implementation source files
├── tests/                          # Automated unit, integration, and benchmark suites
├── resources/                      # Assets (icons, stylesheets, stop-words)
└── data/                           # Local index and history data (.gitignored)
```

---

## Building and Running

Detailed instructions for setting up the Windows 10/11 MSVC 2022 environment and building via command line or VS Code are provided in [**`BUILD_INSTRUCTIONS.md`**](BUILD_INSTRUCTIONS.md).

### Quick Build (x64 Native Tools Command Prompt):
```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.5.3/msvc2022_64"
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
windeployqt build\Debug\SearchEngineApp.exe
.\build\Debug\SearchEngineApp.exe
```

---

## Documentation Index

Comprehensive engineering documents are located in [`docs/`](docs/):

1. [`docs/SRS.md`](docs/SRS.md) — Software Requirements Specification
2. [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — Software Architecture Specification
3. [`docs/DSA_DESIGN.md`](docs/DSA_DESIGN.md) — Custom Data Structures & Algorithms Design
4. [`docs/FILE_INDEXING_ARCHITECTURE.md`](docs/FILE_INDEXING_ARCHITECTURE.md) — Indexing Subsystem Design
5. [`docs/SEARCH_ENGINE_DESIGN.md`](docs/SEARCH_ENGINE_DESIGN.md) — Search Pipeline & Ranking Design
6. [`docs/PERSISTENCE_DESIGN.md`](docs/PERSISTENCE_DESIGN.md) — Persistence & Binary Serialization
7. [`docs/GUI_DESIGN.md`](docs/GUI_DESIGN.md) — Qt 6 Widgets GUI Design
8. [`docs/MODULE_INTERFACES.md`](docs/MODULE_INTERFACES.md) — Module Contracts & Class Interfaces
9. [`docs/TEST_PLAN.md`](docs/TEST_PLAN.md) — QA & Testing Strategy
10. [`docs/PROJECT_STRUCTURE.md`](docs/PROJECT_STRUCTURE.md) — Repository Layout Specification
11. [`docs/DEVELOPMENT_ROADMAP.md`](docs/DEVELOPMENT_ROADMAP.md) — Implementation Roadmap (Phases 0–10)

---

## Current Status

Phase 4.2 (PDF Parser Replacement & Extraction Compliance) is complete with 1,664/1,664 assertions passing. The system is awaiting formal review before Phase 5 (Inverted Index & Tokenizer Pipeline). See [**`DEVELOPMENT_STATUS.md`**](DEVELOPMENT_STATUS.md) and [**`docs/PHASE_4_2_COMPLIANCE_REPORT.md`**](docs/PHASE_4_2_COMPLIANCE_REPORT.md) for details.
