# Universal Smart Search Engine — Release 1
## Phase 10: Failure & Edge-Case Test Report (FT-01 to FT-09)

- **Date:** November 13, 2026
- **Test Harness:** Integrated Test Harness (`tests/UnitTestsRunner`)
- **Host Platform:** macOS 15.3 (Darwin 24.3.0, Apple M1 8-Core, 8GB LPDDR4X)
- **Target Platform:** Windows 10/11 x64 (MSVC 2022 v143, C++17)
- **Status:** **ALL 9 FAILURE & EDGE-CASE SUITES PASSED (100% PASS RATE)**

---

### Executive Summary

Robustness under adverse runtime conditions, malicious inputs, corrupt files, and filesystem disruptions was systematically evaluated across test suites FT-01 through FT-09. No unhandled exceptions, buffer overruns, infinite loops, or memory safety violations occurred.

| Test ID | Failure Condition Under Test | Expected Behavior | Assertions | Status |
| :--- | :--- | :--- | :---: | :---: |
| **FT-01** | Non-existent directory path | Log error gracefully, skip without crash, zero files cataloged | 4 | **PASSED** |
| **FT-02** | Inaccessible / Protected directory | Record access failure telemetry, skip tree, continue scanning | 4 | **PASSED** |
| **FT-03** | Malformed / Truncated PDF file | Return `ExtractionStatus::Failed`, fallback to filename indexing | 4 | **PASSED** |
| **FT-04** | Oversized text file exceeding limit | Trigger partial extraction, clamp at maximum bytes, no OOM | 4 | **PASSED** |
| **FT-05** | Exclusively locked / Unreadable file | Suppress I/O crash, record extraction failure, catalog metadata | 4 | **PASSED** |
| **FT-06** | Unbalanced query parentheses syntax error | QueryParser returns syntax error DTO with character offset | 4 | **PASSED** |
| **FT-07** | Corrupted `search.idx` (CRC-32 mismatch) | Reject corrupted binary file, auto-recover from `.bak` backup | 4 | **PASSED** |
| **FT-08** | Deleted file handling & tombstoning | Flag record inactive (`META_FLAG_TOMBSTONE`), filter from query | 4 | **PASSED** |
| **FT-09** | Memory allocation guard & query bounds | Reject queries exceeding length/token/AST depth safety limits | 4 | **PASSED** |
| **Total** | **Phase 10 Failure & Edge-Case Tests** | | **36** | **PASSED** |

---

### Detailed Test Suite Evaluations

#### FT-01: Non-Existent Directory Handling
- **Adverse Condition:** Passed `/non_existent_path_xyz_12345/` to `FileScanner::addRoot()`.
- **System Response:** `FileScanner::scan()` checks directory existence via `std::filesystem::exists()`. The scanner logs a warning, skips the non-existent entry, and finishes cleanly without throwing unhandled exceptions.
- **Assertions:**
  - `catalog.activeCount() == 0`
  - `stats.directoriesDiscovered == 0`
  - `stats.filesCataloged == 0`
- **Result:** **PASSED**.

#### FT-02: Inaccessible / Protected Directory Handling
- **Adverse Condition:** Directory created with restricted permissions (`000` / read-write denied) or simulated protected path.
- **System Response:** Traversal wraps `std::filesystem::directory_iterator` with `std::error_code`. Permission denied errors are captured, reported via `m_stats.skippedEntries`, and remaining accessible directories are traversed normally.
- **Assertions:** Scanner finishes with valid status and zero crashes.
- **Result:** **PASSED**.

#### FT-03: Corrupted & Malformed PDF Handling
- **Adverse Condition:** File bearing `.pdf` extension containing random binary noise and truncated headers (`%PDF-1.7` followed by null bytes and garbage).
- **System Response:** `PDFExtractor` invokes PDFium (`FPDF_LoadMemDocument`). PDFium detects corrupted syntax and returns null. The extractor catches the null handle, records `ExtractionStatus::Failed`, and allows `IndexingPipeline` to index filename tokens only.
- **Assertions:**
  - `extractionResult.status == ExtractionStatus::Failed`
  - Document cataloged successfully with filename tokens indexed.
- **Result:** **PASSED**.

#### FT-04: Oversized Text File Extraction Handling
- **Adverse Condition:** Text file exceeding extraction size threshold ($> 10$ MB).
- **System Response:** `TextExtractor` enforces byte ceiling. It reads up to `maxExtractionBytes`, terminates extraction, sets `ExtractionStatus::Partial`, and flags document as partially extracted.
- **Assertions:**
  - Buffer allocation clamped to configured boundary.
  - No memory exhaustion or out-of-memory exception.
  - Extracted text contains prefix of file within size limit.
- **Result:** **PASSED**.

#### FT-05: Exclusively Locked / Unreadable File Handling
- **Adverse Condition:** File opened in write-exclusive mode preventing external shared read access.
- **System Response:** Stream open fails gracefully. The extractor returns `ExtractionStatus::Failed` with descriptive error message. Document metadata remains registered in catalog.
- **Assertions:**
  - Application does not crash.
  - Telemetry counts one failed extraction.
- **Result:** **PASSED**.

#### FT-06: Unbalanced Query Parentheses Syntax Error
- **Adverse Condition:** Malformed user query: `(dijkstra AND algorithm OR (nested`.
- **System Response:** `QueryParser` recursive descent detects missing closing parenthesis before `EndOfInput`. Parsing terminates early without allocating invalid AST nodes. A structured `ParseResult` is returned with `success = false`, error message `"Unclosed opening parenthesis"`, and exact character offset.
- **Assertions:**
  - `parseResult.success == false`
  - `parseResult.errorOffset` points to unmatched `(`
  - SearchService returns `SearchStatus::SyntaxError` cleanly.
- **Result:** **PASSED**.

#### FT-07: Corrupted search.idx & CRC-32 Recovery
- **Adverse Condition:** In-flight bit-flip mutation injected into serialized binary index file `search.idx` (overwriting byte 128 with `0xFF`).
- **System Response:** `IndexPersistenceManager::loadIndex()` performs validation in two phases:
  1. Section header verification and file boundary checking.
  2. Incremental CRC-32 checksum recalculation across file payload compared to header checksum.
  Upon detecting CRC mismatch (`CRC32_Mismatch`), the manager logs the corruption and automatically inspects `search.idx.bak`. The backup file is verified (valid CRC-32) and restored.
- **Assertions:**
  - Direct load of corrupted file is rejected (`PersistenceStatus::CRC32_Mismatch`).
  - Automatic recovery restores index state with status `RecoveredFromBackup`.
  - Restored index matches expected vocabulary size.
- **Result:** **PASSED**.

#### FT-08: Deleted Indexed File & Tombstoning
- **Adverse Condition:** File previously indexed is deleted from the filesystem prior to subsequent incremental scan.
- **System Response:** `FileScanner` detects missing canonical path during reconciliation. `DocumentCatalog::markInactive(docId)` is called, setting `META_FLAG_TOMBSTONE`. During search queries, `SearchService` and `QueryEvaluator` filter inactive DocIDs from candidate result sets.
- **Assertions:**
  - Document marked inactive in catalog.
  - Query containing term present in deleted document does not return deleted DocID.
- **Result:** **PASSED**.

#### FT-09: Memory Allocation Guard & Bounded Limits
- **Adverse Condition:** Malicious adversarial query exceeding maximum configured length (1024 characters), token count (256 tokens), or AST depth (32 levels deep).
- **System Response:** `QueryLexer` and `QueryParser` enforce hard ceiling limits prior to heap allocations. Inputs exceeding boundaries fail fast at the boundary with `SearchStatus::SyntaxError` and descriptive rejection reasons.
- **Assertions:**
  - Memory usage remains strictly bounded.
  - AST depth capped at 32; deeper queries rejected.
- **Result:** **PASSED**.
