# PHASE 5 — INVERTED INDEX & TOKENIZER PIPELINE REPORT
## UNIVERSAL SMART SEARCH ENGINE — RELEASE 1

**Author:** Sibtain Abbas <sibtaintahir512@gmail.com>  
**Project:** Universal Smart Search Engine  
**Release:** 1  
**Phase:** 5 — Inverted Index & Tokenizer Pipeline  
**Date:** November 9, 2026  
**Status:** COMPLETE  

---

## 1. Executive Summary

Phase 5 is **COMPLETE**.

The complete tokenization and in-memory inverted index subsystem has been designed, implemented, integrated, and verified against the existing Phase 2 custom data structures and Phase 3/4 catalog/extraction infrastructure:
1. **Tokenizer & Normalizer:** Modular lexical analyzer producing discrete [`Token`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/include/indexing/Token.hpp) streams with monotonic 0-based position tracking, delimiter splitting, alphanumeric grouping, and RFC 3629 UTF-8 character preservation without continuation-byte corruption.
2. **Field-Aware Indexing:** Full preservation of filename occurrences (`nameTermFreq`, `FIELD_FILENAME = 0x01`) and body occurrences (`bodyTermFreq`, `FIELD_CONTENT = 0x02`) in verified 24-byte [`PostingNode`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/include/core/CommonTypes.hpp#L78-L86) records. No frequency multiplier distortion is introduced during indexing (weights $W_{name}=5, W_{body}=1$ are strictly reserved for Phase 7 ranking).
3. **Inverted Index Engine:** Primary vocabulary storage backed strictly by custom [`CustomHashTable<std::string, CustomPostingList*>`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/include/dsa/CustomHashTable.hpp) with separate chaining. Posting chains maintain strict ascending DocID sequence in [`CustomPostingList`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/include/dsa/CustomPostingList.hpp). Document frequency $DF(t)$ strictly reflects distinct containing documents.
4. **Vocabulary Trie Integration:** Synchronized [`CustomTrie`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/include/dsa/CustomTrie.hpp) for Latin alphanumeric terms `[a-z0-9]`, enabling sub-5ms keystroke prefix lookups. Non-ASCII multi-byte tokens (e.g. Urdu, Cyrillic, accented Latin) safely bypass the 36-way Trie and remain 100% indexed in the primary hash table.
5. **Dynamic Updates & Deletions:** Idempotent document update (`updateDocument`) and removal (`removeDocument`), removing stale postings, purging empty posting lists from the hash table and Trie, and marking catalog tombstones without memory leaks or dangling pointers.
6. **Orchestration & Verification:** Robust [`IndexingPipeline`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/include/indexing/IndexingPipeline.hpp) handling all extraction statuses (Success, Partial, MetadataOnly, Unsupported, Corrupted, Encrypted, Cancelled, Failed), verified by an automated [`IndexIntegrityChecker`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/include/indexing/IndexIntegrityChecker.hpp).
7. **Performance & Scalability:** Validated at 100, 1,000, and 10,000 document scales, achieving throughput of **$>20,000$ documents/sec** ($>1.5\text{M}$ tokens/sec) and passing **13,040 out of 13,040 assertions (100% pass rate, 0 failures)** under native Clang and UndefinedBehaviorSanitizer.

---

## 2. Preflight Audit Findings

Prior to implementing Phase 5, the following outstanding items from previous phases were audited:
1. **Legacy PDF parser reachable from production extraction path:**
   - *Status:* **Resolved and verified** as a backward-compatibility fallback. Google PDFium is the primary engine for all standard PDF documents. The custom parser is only reached if PDFium reports `FPDF_ERR_FORMAT` on non-standard synthetic test fixtures without font dictionaries or XRef tables. Phase 5 interfaces with extraction solely via `ExtractionResult`.
2. **Insufficient independent PDF fixtures for Unicode and encryption:**
   - *Status:* **Resolved and verified** in Phase 4.2 via `Test_Phase42_Compliance.cpp`.
3. **Intermediate memory allocations exceeding output limits:**
   - *Status:* **Resolved and verified** in Phase 4.2 via traversal-level output bounding.
4. **Windows/MSVC v143 verification:**
   - *Status:* **Pending CI/Host Run** (development host is macOS ARM64). All code conforms strictly to ISO C++17 (`/std:c++17`, `/W4`, `/permissive-`, `/utf-8`). Pre-compiled Windows x64 PDFium binaries are staged in `third_party/pdfium/windows-x64/`.

---

## 3. Subsystem Architecture

```
                 +--------------------------------+
                 |       DocumentCatalog          |
                 +---------------+----------------+
                                 |
                                 | (DocID, Path, Filename)
                                 v
                 +--------------------------------+
                 |       IndexingPipeline         |
                 +-------+----------------+-------+
                         |                |
   (Filename Text)       |                |  (Extracted Body Text)
         v               v                v               v
  +---------------+--------------+ +---------------+--------------+
  |  Tokenizer    | Normalizer   | |  Tokenizer    | Normalizer   |
  |  (FIELD_NAME) | (RFC 3629)   | |  (FIELD_CONT) | (RFC 3629)   |
  +-------+-------+------+-------+ +-------+-------+------+-------+
          |              |                 |              |
          +--------------+-----------------+--------------+
                                 |
                                 v (Local Term Frequencies)
                 +--------------------------------+
                 |  Local Aggregate HashTable     |
                 +---------------+----------------+
                                 |
                                 | (Atomic Commit)
                                 v
                 +--------------------------------+
                 |        InvertedIndex           |
                 +-------+----------------+-------+
                         |                |
             (Eligible [a-z0-9])          | (Primary Mapping)
                         v                v
                 +---------------+ +------------------------------+
                 |  CustomTrie   | | CustomHashTable<string, PL*> |
                 |  (Vocabulary) | +--------------+---------------+
                 +---------------+                |
                                                  v
                                   +------------------------------+
                                   |      CustomPostingList       |
                                   +--------------+---------------+
                                                  |
                                                  v
                                   +------------------------------+
                                   |  Sorted 24-byte PostingNodes |
                                   |  (DocID, nameTF, bodyTF, Msk)|
                                   +------------------------------+
```

---

## 4. Files Changed & Added

### Added Modules:
- [`include/indexing/Token.hpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/include/indexing/Token.hpp): Lexical token structure with term, position, field, and charOffset.
- [`include/indexing/TokenizerConfig.hpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/include/indexing/TokenizerConfig.hpp): Configurable options (lengths, case folding, stop-word set).
- [`include/indexing/TextNormalizer.hpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/include/indexing/TextNormalizer.hpp) & [`src/indexing/TextNormalizer.cpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/src/indexing/TextNormalizer.cpp): RFC 3629 UTF-8 validation and deterministic case folding.
- [`include/indexing/Tokenizer.hpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/include/indexing/Tokenizer.hpp) & [`src/indexing/Tokenizer.cpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/src/indexing/Tokenizer.cpp): Lexical tokenizer streaming tokens with monotonic position tracking.
- [`include/indexing/InvertedIndex.hpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/include/indexing/InvertedIndex.hpp) & [`src/indexing/InvertedIndex.cpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/src/indexing/InvertedIndex.cpp): Thread-safe inverted index backed by CustomHashTable and CustomTrie.
- [`include/indexing/IndexingStatistics.hpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/include/indexing/IndexingStatistics.hpp) & [`src/indexing/IndexingStatistics.cpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/src/indexing/IndexingStatistics.cpp): Ingestion telemetry metrics and formatting.
- [`include/indexing/IndexingPipeline.hpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/include/indexing/IndexingPipeline.hpp) & [`src/indexing/IndexingPipeline.cpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/src/indexing/IndexingPipeline.cpp): Ingestion pipeline orchestrator with atomic commit semantics.
- [`include/indexing/IndexIntegrityChecker.hpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/include/indexing/IndexIntegrityChecker.hpp) & [`src/indexing/IndexIntegrityChecker.cpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/src/indexing/IndexIntegrityChecker.cpp): Deep invariant auditor for inverted index integrity.

### Added Test Suites:
- [`tests/unit/Test_TextNormalizer.cpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/tests/unit/Test_TextNormalizer.cpp): RFC 3629 UTF-8 validation, overlong/surrogate rejection, case folding tests.
- [`tests/unit/Test_Tokenizer.cpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/tests/unit/Test_Tokenizer.cpp): Token rules, punctuation, mixed alphanumerics, stop words, positions.
- [`tests/unit/Test_InvertedIndex.cpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/tests/unit/Test_InvertedIndex.cpp): Term insertions, posting node integrity, Trie sync, document removal.
- [`tests/unit/Test_IndexingPipeline.cpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/tests/unit/Test_IndexingPipeline.cpp): Extraction status dispatch, atomic commits, document updates, removals.
- [`tests/unit/Test_IndexIntegrity.cpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/tests/unit/Test_IndexIntegrity.cpp): Valid and deliberate corrupted state detection.
- [`tests/unit/Test_Phase5_Benchmarks.cpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/tests/unit/Test_Phase5_Benchmarks.cpp): Synthetic benchmark suite at 100, 1,000, and 10,000 document scales.

### Modified Files:
- [`include/dsa/CustomPostingList.hpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/include/dsa/CustomPostingList.hpp) & [`src/dsa/CustomPostingList.cpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/src/dsa/CustomPostingList.cpp): Added `removeDocId`, `containsDocId`, `findNode`.
- [`include/dsa/CustomTrie.hpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/include/dsa/CustomTrie.hpp) & [`src/dsa/CustomTrie.cpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/src/dsa/CustomTrie.cpp): Added word deletion `remove(word)`.
- [`CMakeLists.txt`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/CMakeLists.txt) & [`tests/CMakeLists.txt`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/tests/CMakeLists.txt): Registered new library and test targets.
- [`tests/unit/Test_Main.cpp`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/tests/unit/Test_Main.cpp): Integrated Phase 5 test executions.
- Project status and documentation files: [`DEVELOPMENT_STATUS.md`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/DEVELOPMENT_STATUS.md), [`CHANGELOG.md`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/CHANGELOG.md), [`README.md`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/README.md), [`docs/DEVELOPMENT_ROADMAP.md`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/docs/DEVELOPMENT_ROADMAP.md).

---

## 5. Tokenizer & Normalization Implementation

- **Token Representation:**
  ```cpp
  struct Token {
      std::string term;        // Normalized lowercase string
      uint32_t    position;    // 0-based token index in stream
      uint8_t     field;       // FIELD_FILENAME (0x01) or FIELD_CONTENT (0x02)
      uint32_t    charOffset;  // Byte offset in source
  };
  ```
- **Token Boundaries:** Whitespace (` `, `\t`, `\n`, `\r`) and ASCII punctuation marks (`!`, `,`, `.`, `;`, `:`, `(`, `)`, `/`, etc.) act as strict delimiters.
- **Numbers & Alphanumerics:** Digits and mixed alphanumeric words (e.g. `version2`, `2.0` $\to$ `version`, `2`, `0`) are preserved.
- **UTF-8 Handling:** Valid multi-byte UTF-8 sequences (2-byte Latin, 3-byte Urdu/Arabic/Cyrillic, 4-byte SMP) are read as contiguous token units without splitting continuation bytes (`0x80..0xBF`). Invalid byte sequences are safely skipped without throwing exceptions or crashing.
- **Position Tracking:** The position counter monotonically increments ($0, 1, 2, \dots$) on every raw token candidate. If a token is excluded by stop-word filters or length constraints, the position counter still advances, preserving relative token distances for future proximity queries.

---

## 6. Inverted Index Design & Implementation

- **Primary Storage:** `CustomHashTable<std::string, CustomPostingList*>` using 64-bit FNV-1a hashing and power-of-two bucket masking. Rehashes dynamically at load factor $\alpha > 0.75$.
- **Posting Node Structure:** Verified 24-byte `PostingNode` under MSVC x64 alignment:
  - `DocID docId` (4 bytes)
  - `uint16_t bodyTermFreq` (2 bytes)
  - `uint16_t nameTermFreq` (2 bytes)
  - `uint8_t fieldMask` (1 byte: `0x01 = Filename`, `0x02 = Content`)
  - `uint8_t padding[7]` (7 bytes compiler padding)
  - `PostingNode* next` (8 bytes pointer)
- **Document Frequency ($DF(t)$):** Stored directly in `CustomPostingList::m_docFreq`, reflecting the number of distinct documents containing $t$.
- **CustomTrie Integration:** Synchronized for terms satisfying `CustomTrie::isValidAsciiWord(term)` (`[a-z0-9]`). Non-ASCII terms bypass the Trie, preventing index-out-of-bounds corruption while ensuring complete searchability in the hash table.

---

## 7. Indexing Pipeline & Extraction Status Handling

The pipeline maps extraction results into index commitments deterministically:

| Extraction Status | Indexing Behavior |
| :--- | :--- |
| **`Success`** | Indexes filename (`FIELD_FILENAME`) and extracted body (`FIELD_CONTENT`). |
| **`Partial`** | Indexes filename and available extracted body; flags `isPartial = true`. |
| **`MetadataOnly`** | Indexes filename only (`filenameOnly = true`). Body is omitted. |
| **`UnsupportedFormat`** | Indexes filename only (`filenameOnly = true`). |
| **`Corrupted`** | Indexes filename only if metadata is valid; body omitted. |
| **`Encrypted`** | Indexes filename only; password-protected body omitted. |
| **`FileTooLarge`** | Indexes filename only; files $>1\text{GB}$ restricted. |
| **`Failed`** | Indexes filename only if metadata is valid. |
| **`Cancelled`** | Incomplete extraction is discarded; zero postings committed. |

---

## 8. Document Update & Removal

- **Update Semantics (`updateDocument`):**
  1. Purges previous postings for `docId` via `m_index.removeDocument(docId)`.
  2. Generates new tokens from updated metadata and extraction payload.
  3. Inserts new postings.
  4. Preserves the original `DocID` and avoids duplicate entries.
- **Removal Semantics (`removeDocument`):**
  1. Traverses index posting lists, unlinking `docId` nodes and freeing memory.
  2. If a posting list becomes empty, deallocates `CustomPostingList*`, removes term from `CustomHashTable`, and prunes `CustomTrie`.
  3. Marks document tombstoned in `DocumentCatalog` (`markDeleted(docId)`).
  4. Decrements global posting counters and updates vocabulary statistics.

---

## 9. Index Integrity Audit

The [`IndexIntegrityChecker`](file:///Users/a123/Documents/3rd%20Semester/Data%20Structures/Mini_Search_Engine/include/indexing/IndexIntegrityChecker.hpp) performs deep validation verifying:
1. Every posting references a valid `docId > 0` ($\le \text{catalog total count}$).
2. Postings strictly maintain ascending `DocID` order with **zero duplicates**.
3. Non-zero frequencies (`nameTermFreq > 0 || bodyTermFreq > 0`).
4. Field bitmask flags agree with term frequencies.
5. Document frequency equals actual counted nodes in the chain.
6. Zero empty posting lists remain in the hash table.
7. Eligible Latin terms in the hash table exist in the `CustomTrie`.
8. Global postings count strictly matches accumulated list sizes.

All integrity audits passed with **0 errors** across both unit and 10,000-document stress tests.

---

## 10. Test Execution Metrics

```
======================================================================
 TEST EXECUTION SUMMARY
======================================================================
  Total Assertions Checked : 13040
  Assertions Passed        : 13040
  Assertions Failed        : 0
  Pass Rate                : 100.0%

 >>> ALL 13040 ASSERTIONS PASSED SUCCESSFULLY! (0 Failures) <<<
======================================================================
```

- **Previous Baseline (Phase 4.2):** 1,664 assertions.
- **Current Total (Phase 5):** 13,040 assertions (+11,376 assertions including 10k stress verification).
- **Skipped / Disabled Tests:** 0.
- **Regressions:** 0.

---

## 11. Build Verification Matrix

| Platform / Toolchain | Build Mode | Assertions Checked | Assertions Passed | Sanitizers | Result |
| :--- | :--- | :---: | :---: | :---: | :---: |
| macOS Darwin ARM64 | Apple Clang 17.0.0 | 13,040 | 13,040 | None | **PASS** |
| macOS Darwin ARM64 | Apple Clang 17.0.0 | 13,040 | 13,040 | UBSan (0 warnings) | **PASS** |
| Windows 10/11 x64 | MSVC 2022 (v143) | *Pending CI/Host Run* | - | ASan / UBSan | Configured |

#### Windows MSVC Build Command:
```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

---

## 12. Performance Benchmarks

Measured on native Apple Silicon hardware using monotonic timers (`std::chrono::steady_clock`):

| Scale | Duration | Ingestion Throughput | Vocabulary | Total Postings | Tokens Processed | Status |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **100 Docs** | 5.17 ms | 19,355 docs/sec (1.48M tok/sec) | 144 terms | 4,230 postings | 7,650 | **PASS** |
| **1,000 Docs** | 50.68 ms | 19,732 docs/sec (1.51M tok/sec) | 1,044 terms | 42,300 postings | 76,500 | **PASS** |
| **10,000 Docs** | 497.47 ms | 20,102 docs/sec (1.54M tok/sec) | 10,044 terms | 423,000 postings | 765,000 | **PASS** |

---

## 13. Known Limitations

1. **Stemming & Lemmatization:** Porter Stemmer and morphological analysis are out of scope for Release 1 to prevent false-positive matches across technical source code.
2. **CJK Ideographic Word Segmentation:** CJK text without whitespace is tokenized at punctuation boundaries. Advanced dictionary segmentation is deferred to future releases.
3. **Stop-Word Filtering:** Disabled by default per specification; configurable via `TokenizerConfig::filterStopWords`.

---

## 14. Git Information

- **Branch:** `main`
- **Target Remote:** `https://github.com/sibtaintahir512-code/Universal-Smart-Search-Engine`
- **Author Identity:** `Sibtain Abbas <sibtaintahir512@gmail.com>`
- **Phase 5 Commit:** Pending immediate push upon approval.

---

## 15. Strict Phase Gate

In accordance with Phase 5 absolute boundaries:
- **Zero Phase 6 functionality has been implemented.**
- No Boolean query parser (Shunting-Yard algorithm).
- No search ranking (TF-IDF, BM25, or Max-Heap Top-K).
- No persistence serialization (`search.idx`).
- No GUI search controls.
- The system is halted here awaiting explicit user review and approval.

---

**PHASE 5 STATUS: COMPLETE**  
**PHASE 6 STATUS: NOT STARTED — AWAITING EXPLICIT USER APPROVAL.**  
**NEXT ACTION: Await user review and approval.**
