# Universal Smart Search Engine — Release 1
# Repair Changelog & Engineering Corrections

- **Release Cycle:** Stabilization & Release 1 Readiness
- **Auditor & Implementer:** Sibtain Abbas (`sibtaintahir512@gmail.com`)
- **Repository:** `https://github.com/sibtaintahir512-code/Universal-Smart-Search-Engine`

---

### Summary of Engineering Corrections

#### 1. File Scanner DocID Propagation (`src/indexing/FileScanner.cpp`)
- **Problem:** When `FileScanner::scan()` registered discovered files in `DocumentCatalog`, the returned `DocID` was discarded, leaving `record.metadata.docId` initialized to `INVALID_DOC_ID` (0). Callbacks attempting to immediately index files received `docId = 0`, causing premature aborts.
- **Correction:** Captured the returned `DocID` from `catalog.registerDocument(...)` and assigned it to `record.metadata.docId` before dispatching the `onFile` callback.
- **Affected Files:** `src/indexing/FileScanner.cpp`.

#### 2. SearchService RankingConfig Initialization (`tests/integration/Test_Integration_IT05_IT08.cpp`)
- **Problem:** `IT-05` configured a standalone `ranking::RankingEngine` object, while `search::SearchService` constructed its own internal ranking engine using default `SearchConfig` (`RankingMode::DocIdOrder`). This caused BM25 queries in `IT-05` to execute in unranked DocID order.
- **Correction:** Configured `SearchConfig.rankingConfig` with BM25 parameters and passed it directly to the `SearchService` constructor.
- **Affected Files:** `tests/integration/Test_Integration_IT05_IT08.cpp`.

#### 3. ThemeManager CSS Syntax Correction (`src/gui/ThemeManager.cpp`)
- **Problem:** Application logs displayed `Unknown property border-top-right-left` on launch due to a typo in the table header stylesheet.
- **Correction:** Replaced `border-top-right-left: 6px;` with standard `border-top-right-radius: 6px;` in both Dark and Light theme palettes.
- **Affected Files:** `src/gui/ThemeManager.cpp`.

#### 4. macOS Native App Bundle & Standalone Deployment (`CMakeLists.txt`)
- **Problem:** On macOS, `SearchEngineApp` built as a raw UNIX CLI binary instead of a native `.app` bundle, preventing standard launch from Finder, Spotlight, or Launchpad.
- **Correction:** Added `MACOSX_BUNDLE TRUE`, bundle name, identifier, and version metadata in CMake. Added `macdeployqt` packaging support and embedded `libpdfium.dylib` into `Contents/Frameworks/`.
- **Affected Files:** `CMakeLists.txt`.

#### 5. 20-Step End-to-End Workflow Verification Suite (`tests/integration/Test_EndToEndWorkflow.cpp`)
- **Addition:** Authored comprehensive 20-step end-to-end integration test creating a realistic dataset of 17 distinct file formats, verifying scanning, extraction, Boolean search, ranking, autocomplete, preview, file deletion/modification, persistence, and 100% round-trip equivalence.
- **Affected Files:** `tests/integration/Test_EndToEndWorkflow.cpp`, `tests/CMakeLists.txt`, `tests/unit/Test_Main.cpp`.
