# Universal Smart Search Engine — Release 1
# Comprehensive Defect Register & Resolution Log

- **Release:** Release 1
- **Updated:** November 13, 2026
- **Status:** **ALL DISCOVERED DEFECTS RESOLVED & VERIFIED**

---

### Defect Classification & Priority Scale
- **P0 (Critical):** Data loss, silent index corruption, unrecoverable crashes, security holes.
- **P1 (High):** Core search, indexing, persistence, or ranking functionality broken.
- **P2 (Medium):** GUI workflow disruptions, thread termination stalls, model binding mismatches.
- **P3 (Low):** Minor performance regressions, non-breaking warnings, usability quirks.
- **P4 (Cosmetic):** Formatting inconsistencies, CSS styling glitches, log formatting.

---

### Master Defect Log

| Defect ID | Priority | Subsystem | Description | Root Cause | Resolution | Status |
| :---: | :---: | :--- | :--- | :--- | :--- | :---: |
| **DEF-01** | **P1** | File Indexing | Benchmark & scan callbacks received uninitialized `DocID = 0` | `FileScanner::scan()` registered document in catalog but did not store the returned `DocID` in `record.metadata.docId`. | Assigned `record.metadata.docId = catalog.registerDocument(...)` in `FileScanner.cpp`. | **CLOSED** |
| **DEF-02** | **P1** | Search & Ranking | IT-05 integration test defaulted to DocIdOrder instead of BM25 | `SearchService` was initialized with default `SearchConfig` instead of the configured BM25 ranking mode. | Passed configured `SearchConfig` directly into `SearchService` constructor in test harness. | **CLOSED** |
| **DEF-03** | **P3** | Core DSA | Implicit sign conversion compiler warning under Apple Clang | Iterating `const char` into `unsigned char` in `CustomHashTable.hpp` triggered `-Wsign-conversion`. | Added explicit `static_cast<unsigned char>` cast in hashing loop. | **CLOSED** |
| **DEF-04** | **P4** | Desktop GUI | Theme styling warning on app startup | Typo `border-top-right-left: 6px;` in `ThemeManager.cpp` Dark and Light theme style definitions. | Corrected to valid Qt CSS rule `border-top-right-radius: 6px;`. | **CLOSED** |
| **DEF-05** | **P2** | Build & Packaging | macOS binary was not packaged as a native double-clickable `.app` bundle | CMake lacked `MACOSX_BUNDLE` property on `SearchEngineApp` target for macOS. | Configured `MACOSX_BUNDLE` properties, bundle identifier, version strings, and `macdeployqt` integration. | **CLOSED** |

---

### Defect Resolution Metrics
- **Total Defects Identified:** 5
- **Total Defects Resolved:** 5 (100%)
- **Unresolved / Open Defects:** 0
- **Regression Tests Added:** End-to-end 20-step workflow, IT-01 through IT-08, and FT-01 through FT-09.
