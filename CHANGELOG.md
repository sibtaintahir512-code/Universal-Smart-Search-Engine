# Changelog
All notable changes to the **Universal Smart Search Engine** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
