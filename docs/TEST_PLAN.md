# Quality Assurance & Test Plan (TEST_PLAN)
## Project: Universal Smart Search Engine
### Release 1 — Local Universal File Search Engine
**Document Version:** 1.0.0  
**Target Platform:** Windows Desktop (C++17 / MSVC 2022 x64 / Qt 6 Widgets / CMake)  
**Academic Context:** University Data Structures & Algorithms Coursework  
**Status:** Approved for Architectural Baseline  

---

## 1. Testing Strategy Overview

The testing strategy establishes a multi-tiered verification framework designed to guarantee algorithmic correctness, memory safety, and UI stability:

```
┌────────────────────────────────────────────────────────┐
│           Level 4: End-to-End & Performance            │
│   (Synthetic 100k Corpora, Latency Timers, Stress)     │
├────────────────────────────────────────────────────────┤
│           Level 3: Failure & Edge Case Testing         │
│   (Locked Files, Corrupted Indices, ACL Errors)        │
├────────────────────────────────────────────────────────┤
│           Level 2: Subsystem Integration Testing       │
│   (Scanner-to-Catalog, Shunting-Yard-to-Postings)      │
├────────────────────────────────────────────────────────┤
│           Level 1: Pure DSA Unit Testing               │
│   (CustomHashTable, PostingList, Trie, MaxHeap, etc.)  │
└────────────────────────────────────────────────────────┘
```

---

## 2. Level 1: Pure DSA Unit Testing

Every custom data structure is subjected to five categories of unit tests in isolation, utilizing a native test runner or Catch2/GoogleTest framework:

### 2.1 Test Matrix Across Custom Structures

| Test ID | Target Structure | Category | Scenario / Inputs | Expected Output / Invariant |
| :--- | :--- | :--- | :--- | :--- |
| **UT-LL-01** | `CustomSinglyLinkedList` | Normal | `push_back(10)`, `push_back(20)` | Traversal yields `10 -> 20 -> nullptr`; `size == 2` |
| **UT-LL-02** | `CustomSinglyLinkedList` | Boundary | Pop/Delete on empty list | Graceful false/no-op; zero null-pointer crashes |
| **UT-LL-03** | `CustomSinglyLinkedList` | Memory | Allocate 100,000 nodes and call destructor | All 100,000 nodes freed; non-recursive loop avoids stack overflow |
| **UT-DA-01** | `CustomDynamicArray` | Normal | Push 20 elements beyond initial capacity (8) | Array dynamically resizes via $1.5\times$ policy; elements intact |
| **UT-DA-02** | `CustomDynamicArray` | Boundary | Access index `size` | Throws `std::out_of_range` exception cleanly |
| **UT-DA-03** | `CustomDynamicArray` | Semantics | Move construct from existing array | Target receives data pointer; source pointer reset to `nullptr` |
| **UT-HT-01** | `CustomHashTable` | Normal | Insert 5,000 unique key-value pairs | All 5,000 keys retrieved in $O(1)$ average time |
| **UT-HT-02** | `CustomHashTable` | Collision | Insert keys forced to same bucket | Separate chaining links entries; retrieval resolves collision chain |
| **UT-HT-03** | `CustomHashTable` | Rehash | Insert elements until $\alpha > 0.75$ | Capacity doubles ($M \times 2$); all existing keys re-spliced safely |
| **UT-PL-01** | `CustomPostingList` | Set Math | Intersect list `[1, 5, 8]` with list `[2, 5, 8, 12]` | Returns new list `[5, 8]` in linear time |
| **UT-PL-02** | `CustomPostingList` | Set Math | Union list `[1, 5]` with list `[2, 5, 9]` | Returns sorted unique list `[1, 2, 5, 9]` |
| **UT-PL-03** | `CustomPostingList` | Set Math | Difference `[1, 2, 3, 4]` minus `[2, 4]` | Returns list `[1, 3]` |
| **UT-PL-04** | `PostingNode` | Layout | `static_assert(sizeof(PostingNode) == 24)` | Verified 24-byte layout on MSVC x64 (4+2+2+1+7pad+8) |
| **UT-TR-01** | `CustomTrie` | Normal | Insert `algorithm`, `algebra`, `binary` | Prefix search for `alg` returns both words |
| **UT-TR-02** | `CustomTrie` | Boundary | Prefix search for non-existent prefix `xyz` | Returns `nullptr` / empty vector in $O(P)$ time |
| **UT-TR-03** | `CustomTrie` | Ranking | Insert 10 words with prefix `th`; request Top-5 | Returns 5 highest search-frequency words only |
| **UT-TR-04** | `CustomTrie` | Multilingual | Ingest Urdu token `اردو` into search engine | Token bypasses 36-way Trie safely; indexed in `CustomHashTable` |
| **UT-MH-01** | `CustomMaxHeap` | Normal | Push unsorted scores `[4.2, 9.8, 1.5, 7.3]` | `extractMax()` returns `9.8`, followed by `7.3` |
| **UT-MH-02** | `CustomMaxHeap` | Tie-Break | Two nodes with identical score `8.5` | Node with more recent timestamp extracted first |
| **UT-MH-03** | `CustomMaxHeap` | Linear Build | Call `buildHeap()` on 50,000 candidate nodes | Valid heap invariant verified in $O(M)$ time |
| **UT-ST-01** | `CustomStack` | Normal | Push `(`, `A`, `AND`, `B`; pop in reverse | Strict LIFO order verified; size decrements accurately |
| **UT-QU-01** | `CustomQueue` | Normal | Enqueue paths `A`, `B`, `C`; dequeue | Strict FIFO order verified; `head` and `tail` reset upon empty |

---

## 3. Level 2: Subsystem Integration Testing

| Integration Test ID | Interacting Modules | Test Scenario | Verification Criteria |
| :--- | :--- | :--- | :--- |
| **IT-01** | `FileScanner` + `FileMetadataManager` | Scanner discovers folder containing mixed file types | Metadata manager classifies extensions into correct `FileCategory` |
| **IT-02** | `ContentExtractor` + `TextProcessor` | Plain text file with punctuation and stop words | TextProcessor receives raw stream, strips punctuation, and removes stop words |
| **IT-03** | `TextProcessor` + `InvertedIndex` | Token stream from single document ingested | Terms appear in `CustomHashTable`; `DocID` appended to `CustomPostingList` |
| **IT-04** | `QueryParser` + `BooleanSearchEngine` | Complex query: `(cpp OR python) AND guide` | Shunting-Yard generates RPN; boolean engine returns intersection of union |
| **IT-05** | `RankingEngine` + `CustomMaxHeap` | Candidate set of 500 documents evaluated | TF-IDF scores computed; Max-Heap yields Top-50 results in descending order |
| **IT-06** | `IndexStorage` + Full In-Memory State | Save 10,000 files to disk and deserialize | Restored `DocumentCatalog` and `InvertedIndex` match pre-save state 100% |
| **IT-07** | `SearchController` + Qt GUI | Search query submitted via GUI search box | Worker completes search; results emitted via Qt signal and rendered in table |
| **IT-08** | `DocumentCatalog` + `InvertedIndex` | Delete 200 files, trigger tombstone compaction | Reference remapping updates all surviving postings; exactly zero dangling references |

---

## 4. Level 3: Failure & Edge-Case Resilience Testing

| Failure Test ID | Failure Injection | System Response & Expected Behavior |
| :--- | :--- | :--- |
| **FT-01** | User selects non-existent directory path | GUI displays warning; scanner ignores path and does not crash |
| **FT-02** | Scanner encounters Windows `System Volume Information` | `std::error_code` catches access denial; folder skipped without halting scan |
| **FT-03** | Corrupted / malformed PDF document encountered | Extractor logs parsing warning; sets status to `ExtractionStatus::Failed`; scan continues |
| **FT-04** | Plain text file exceeding 50 MB threshold | Extractor streams first 50 MB, marks status as `Truncated`, and indexes partial tokens |
| **FT-05** | Active file locked exclusively by another process | Sharing violation handled gracefully; file metadata indexed while content is skipped |
| **FT-06** | Query with unbalanced parentheses: `(cpp AND (dsa)` | Parser catches missing `)` at position 13; shows friendly red syntax alert in UI |
| **FT-07** | `search.idx` binary file corrupted by modifying bytes | CRC32 mismatch detected on startup; user prompted with clean rebuild dialog |
| **FT-08** | User deletes an indexed file from filesystem | Incremental scan identifies missing path, sets `is_active = 0` (soft tombstone) |
| **FT-09** | Out-of-memory simulation during large scan | Allocation failure caught cleanly; active indexing halted with diagnostic message |

---

## 5. Level 4: Performance & Benchmarking Plan

All performance targets stated in this document are **Proposed Engineering Targets** to be formally verified in Phase 10. They represent design goals rather than proven guarantees.

### 5.1 Standardized Benchmark Specification
To avoid anecdotal performance claims, all benchmarks must adhere to the following setup:
- **Reference Hardware:** 6-Core / 12-Thread x64 CPU @ 2.5 GHz+ (e.g., AMD Ryzen 5 or Intel Core i5/i7), 16 GB DDR4/DDR5 RAM, PCIe NVMe M.2 SSD, Windows 10/11 64-bit.
- **Corpus Characteristics:** Synthetic standardized test corpora:
  - **Small Corpus:** 1,000 files ($\approx 25\ \text{MB}$ total, mixed text, code, images).
  - **Medium Corpus:** 10,000 files ($\approx 250\ \text{MB}$ total).
  - **Stress Corpus:** 100,000 files ($\approx 2.5\ \text{GB}$ total; 60% text/code, 25% media, 15% others; $\approx 50,000$ unique vocabulary terms).
- **Cache Conditions:**
  - *Cold Cache:* Index deserialized from disk immediately following application boot without prior OS cache warming.
  - *Warm Cache:* Repeated in-memory queries where the index structures reside entirely in RAM.
- **Measurement Protocol:** High-resolution timers via `std::chrono::high_resolution_clock`. Each test must be repeated for a **minimum of 5 runs**, discarding minimum and maximum outliers, reporting the median $\pm$ standard deviation.

### 5.2 Proposed Performance Targets (Subject to Phase 10 Verification)

| Metric | Corpus Size | Proposed Target Baseline | Cache State | Measurement Instrument |
| :--- | :--- | :--- | :--- | :--- |
| **Single-Word Search Latency** | 100,000 files | $\le 5.0\ \text{ms}$ (Proposed Target) | Warm memory | High-resolution microsecond stopwatch |
| **3-Word Boolean Query Latency** | 100,000 files | $\le 50.0\ \text{ms}$ (Proposed Target) | Warm memory | Execution stopwatch (median of 5 runs) |
| **Trie Autocomplete Latency** | 50,000 terms | $\le 5.0\ \text{ms}$ per keystroke (Proposed) | Warm memory | Qt event loop timing telemetry |
| **Cold Startup Index Deserialization** | 100,000 files | $\le 3.0\ \text{seconds}$ (Proposed Target) | Cold disk cache | Startup stopwatch logging |
| **Incremental Re-Index Sync (1 file changed)** | 100,000 files | $\le 250.0\ \text{ms}$ (Proposed Target) | Warm OS cache | Timestamp delta comparator timer |
| **Peak Working Set RAM** | 100,000 files | $\le 350.0\ \text{MB}$ (Proposed Target) | Resident memory | Windows Task Manager / VMMap |
| **Memory Leaks on Clean Shutdown** | Full Session | **Exactly 0 bytes** | Shutdown | MSVC CRT Leak Dump / Dr. Memory |
