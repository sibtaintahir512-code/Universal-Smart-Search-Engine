# Universal Smart Search Engine — Release 1
# User Manual & Operator Guide

---

## 1. Introduction

The **Universal Smart Search Engine** is a high-performance, privacy-focused, offline desktop search application engineered in C++17 and Qt 6. It indexes your local storage drives and directories, enabling sub-millisecond Boolean search, full-text content retrieval, relevance ranking (BM25 and TF-IDF), and instant file previews across tens of thousands of documents without relying on external cloud services or network access.

---

## 2. System Requirements & Installation

### 2.1 Hardware Requirements
- **Processor:** 64-bit x86-64 processor (Intel Core i3 / AMD Ryzen 3 or higher) or Apple Silicon (M1/M2/M3 via native build).
- **RAM:** 4 GB minimum (8 GB recommended for indexing $>100,000$ documents).
- **Storage:** 200 MB free disk space for application and libraries; index cache consumes approximately 1–2 MB per 10,000 indexed documents.

### 2.2 Operating System
- **Windows:** Windows 10 (64-bit) or Windows 11 (64-bit).
- **macOS (Developer/Host Build):** macOS 13.0+ (Ventura, Sonoma, Sequoia).
- **Linux:** Ubuntu 22.04 LTS or compatible modern distribution.

### 2.3 Installation Steps
1. Download the release package (`UniversalSmartSearchEngine-Setup.zip` or build from source using CMake).
2. Extract the package to your preferred directory (e.g., `C:\Program Files\UniversalSearchEngine`).
3. Ensure the PDFium shared library (`pdfium.dll` on Windows, `libpdfium.dylib` on macOS) is in the application folder or system library path.
4. Launch `UniversalSmartSearchEngine.exe` (or `./build/app/UniversalSmartSearchEngine`).

---

## 3. Getting Started: First Scan & Indexing

When launching the application for the first time, you will be greeted by the modern Qt 6 interface.

```
+----------------------------------------------------------------------------+
| Universal Smart Search Engine - [Dark Theme]                     [_][O][X] |
+----------------------------------------------------------------------------+
| Search: [ dijkstra algorithm                                 ] [ Search ]  |
| Mode:   (o) BM25  ( ) TF-IDF  ( ) DocID      Scope: [ All Categories   v] |
+----------------------------------------------------------------------------+
| Index Status: 10,480 documents indexed (15,200 terms)         [ Reindex ]  |
+----------------------------------------------------------------------------+
| Rank | Filename           | Path                     | Size   | Score      |
+------+--------------------+--------------------------+--------+------------+
| 1    | dijkstra.cpp       | C:/src/graph/dijkstra.cpp| 4.2 KB | 28.45      |
| 2    | graph_guide.pdf    | C:/docs/graph_guide.pdf  | 1.8 MB | 19.12      |
| 3    | algorithms.docx    | C:/docs/algorithms.docx  | 450 KB | 14.80      |
+----------------------------------------------------------------------------+
| Preview:                                                                   |
| #include <iostream>                                                        |
| // Implements Dijkstra's Shortest Path Algorithm                           |
+----------------------------------------------------------------------------+
```

### Adding Folders to Index:
1. Click **File -> Index Directory...** (or press `Ctrl+O`).
2. Select the directory you wish to index (e.g., `Documents` or a source code repository).
3. The background indexing worker starts automatically.
4. The status bar reflects real-time progress:
   - Scanning directories and extracting metadata.
   - Parsing text and tokenizing document terms.
   - Ingesting into the custom Trie-indexed Inverted Index.
5. Once complete, your index is saved automatically to `search.idx` with atomic persistence and CRC-32 checksum protection.

---

## 4. Search Query Syntax Guide

The search engine features a full Boolean expression parser supporting lexical tokenization, case-folding, field-restricted search, and operator precedence.

### 4.1 Basic Single-Term Search
Enter any word to search across all indexed filenames and document contents:
```text
algorithm
```
*Matches any document containing "algorithm" in its filename or extracted text.*

### 4.2 Boolean Operators (AND, OR, NOT)
Combine terms using standard Boolean operators. Operators are case-insensitive.
- **AND (Conjunction):** Both terms must appear in the document:
  ```text
  dijkstra AND algorithm
  ```
  *(Note: Whitespace between adjacent words acts as an implicit AND: `dijkstra algorithm`)*
- **OR (Disjunction):** Either term may appear:
  ```text
  python OR cplusplus
  ```
- **NOT (Negation):** Exclude documents containing the term:
  ```text
  graph AND NOT directed
  ```

### 4.3 Grouping with Parentheses
Use parentheses to dictate operator evaluation precedence:
```text
(compiler OR interpreter) AND (llvm OR clang) AND NOT legacy
```

### 4.4 Field-Restricted Search
Target specific document attributes using field prefixes:
- **`name:`** Restricts matching to the document filename only:
  ```text
  name:main AND content
  ```
- **`body:`** Restricts matching to document contents only:
  ```text
  body:quicksort
  ```

---

## 5. Relevance Ranking Models

You can select the desired ranking algorithm using the radio buttons above the search bar:

1. **BM25 (Best Matching 25 — Recommended):**
   - State-of-the-art probabilistic information retrieval model.
   - Normalizes for document length (prevents long documents from artificially dominating results).
   - Weights filename matches $5\times$ heavier than body matches by default.
2. **TF-IDF (Term Frequency – Inverse Document Frequency):**
   - Classic vector space ranking.
   - Rewards documents containing rare, specialized query terms while discounting common words.
3. **DocID Order:**
   - Unranked raw retrieval order (sorted by ascending Document ID).
   - Fastest possible mode; ideal when you want to inspect documents in discovery order.

---

## 6. Result Filtering, Sorting & Preview

- **Category Filtering:** Use the category dropdown to filter results by file type:
  - *All Categories*
  - *Documents* (`.pdf`, `.docx`, `.xlsx`, `.txt`, `.rtf`, `.md`)
  - *Source Code* (`.cpp`, `.h`, `.py`, `.java`, `.rs`, `.c`, `.js`, `.ts`)
  - *Media & Archives* (`.zip`, `.tar`, `.png`, `.jpg`, `.mp4`)
- **Column Sorting:** Click on any table column header (Filename, Path, Size, Score) to sort ascending or descending.
- **Instant Preview:** Click any row in the search results table to view a formatted text snippet in the bottom preview pane.
- **Open File:** Double-click any row to launch the file directly in your operating system's default viewer (e.g., VS Code, Adobe Acrobat, or Microsoft Word).

---

## 7. Index Maintenance, Compaction & Persistence

- **Automatic Persistence:** Every indexing operation writes state to disk atomically using temporary files and CRC-32 checksums (`search.idx`). A backup file (`search.idx.bak`) is automatically maintained.
- **Crash Recovery:** If an unexpected power outage occurs during a write, the engine detects CRC mismatch on startup and restores from the `.bak` file seamlessly.
- **Index Compaction:** When files are deleted or modified on disk, old document records become tombstones. Click **Tools -> Compact Index** to trigger garbage collection, renumbering DocIDs contiguously and reclaiming storage.

---

## 8. Troubleshooting & FAQ

**Q: Why does a search return zero results?**
- Verify spelling and capitalization (search is case-insensitive, but terms must match root words).
- If using `AND NOT`, ensure you did not accidentally exclude all matching documents.
- Check if the target folder was added to the indexed roots list.

**Q: A file was modified on disk. How do I update the index?**
- Click **Reindex** (or press `F5`). The `FileChangeDetector` performs an incremental scan based on file timestamps and sizes, re-extracting only modified files in milliseconds.

**Q: Is my data uploaded anywhere?**
- **No.** The Universal Smart Search Engine operates 100% locally on your computer with zero telemetry, zero cloud calls, and zero external network sockets.
