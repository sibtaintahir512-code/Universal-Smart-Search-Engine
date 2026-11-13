# Universal Smart Search Engine — Release 1
# Academic Demonstration & Evaluation Script

- **Purpose:** A structured, reproducible 10-step evaluation protocol for academic evaluators, professors, and technical reviewers to rigorously inspect and verify all features of the Universal Smart Search Engine.
- **Estimated Demonstration Time:** 15–20 minutes.
- **Repository:** `https://github.com/sibtaintahir512-code/Universal-Smart-Search-Engine`

---

### Step 1: Architectural Orientation & Custom DSA Verification

**Objective:** Prove that the search engine is built from scratch without standard STL containers for core indexing algorithms.

1. Open a terminal in the project root directory.
2. Run the static container audit command:
   ```bash
   grep -rnE "std::(vector|list|map|unordered_map|set|unordered_set|priority_queue|stack|queue)" include/dsa/ include/indexing/ include/ranking/
   ```
3. **Evaluator Observation:** Zero occurrences are returned.
4. Inspect `include/dsa/`:
   - `CustomDynamicArray.hpp`: Dynamic array with geometric expansion.
   - `CustomHashTable.hpp`: Collision-chained hash map for path $\to$ DocID mapping.
   - `CustomTrie.hpp`: Prefix trie storing dictionary terms and pointing to posting lists.
   - `CustomPostingList.hpp`: Linked-list posting nodes storing term frequencies and field masks.
   - `CustomMinHeap.hpp` / `CustomMaxHeap.hpp`: Binary heaps powering $O(M \log K)$ bounded Top-K selection.

---

### Step 2: Automated Integrated Test Suite Execution

**Objective:** Validate that all unit, integration, and failure test suites pass cleanly with 100% success rate.

1. Run the unified test runner:
   ```bash
   ./build/tests/UnitTestsRunner
   ```
2. **Evaluator Observation:**
   - Total Assertions Checked: **15,549**
   - Assertions Passed: **15,549**
   - Assertions Failed: **0**
   - All phases (Phase 2 Custom DSA through Phase 10 Benchmarks) display `[PASSED]`.

---

### Step 3: Launching the Desktop Application & GUI Inspection

**Objective:** Inspect the Qt 6 Widgets graphical user interface and verify modern design standards.

1. Launch the desktop GUI:
   ```bash
   ./build/app/UniversalSmartSearchEngine
   ```
2. **Evaluator Observation:**
   - GUI displays a polished, high-contrast dark theme.
   - Search bar with clear button, category filter dropdown, ranking mode selectors (BM25, TF-IDF, DocID Order).
   - Results table with sorting columns (Rank, Filename, Path, Size, Score).
   - Bottom preview dock for text extraction inspection.

---

### Step 4: Indexing a Multi-Format Document Corpus

**Objective:** Demonstrate directory scanning, change detection, and multi-format text extraction (TXT, CPP, PDF, DOCX).

1. Click **File -> Index Directory...**.
2. Select the `tests/fixtures/` directory or any local test folder.
3. Observe the progress dialog and status bar.
4. **Evaluator Observation:**
   - Files are discovered via breadth-first search (`CustomQueue`).
   - File metadata is extracted and registered into `DocumentCatalog`.
   - Content is extracted concurrently; progress bar updates smoothly without locking the GUI.
   - Total files indexed and term count update instantly upon completion.

---

### Step 5: Single-Term & Prefix Autocomplete Search

**Objective:** Demonstrate sub-millisecond retrieval and prefix trie autocomplete.

1. Type the prefix `alg` into the search bar.
2. Observe autocomplete dropdown suggesting `algorithm`, `algebra`, etc., populated directly from `CustomTrie::findPrefix()`.
3. Press `Enter` to search for `algorithm`.
4. **Evaluator Observation:**
   - Matches return instantaneously ($< 1$ ms latency).
   - Results display filename, directory path, file size, and calculated relevance score.

---

### Step 6: Boolean Query Processing (AND, OR, NOT, Parentheses)

**Objective:** Demonstrate recursive descent AST parsing, operator precedence, and set operations.

1. Execute a conjunction query:
   ```text
   graph AND dijkstra
   ```
   *Verify only documents containing both terms appear.*
2. Execute a disjunction query:
   ```text
   python OR cplusplus
   ```
   *Verify union of matching documents appears.*
3. Execute a compound precedence query with negation:
   ```text
   (graph OR tree) AND NOT directed
   ```
4. **Evaluator Observation:**
   - Correct subset of documents is returned.
   - Evaluator query execution telemetry in status bar shows AST evaluation duration in microseconds.

---

### Step 7: Relevance Ranking Comparison (BM25 vs TF-IDF vs DocID)

**Objective:** Validate mathematical relevance scoring and Top-K heap selection.

1. Execute search query: `target` (or a known repeated term).
2. Toggle **BM25**:
   - High-frequency term occurrences and filename matches score highest.
   - Document 100 sits at Rank 1.
3. Toggle **TF-IDF**:
   - Classic vector space model scores documents based on term frequency and inverted doc frequency.
4. Toggle **DocID Order**:
   - Documents are sorted in natural ascending DocID order ($1, 2, 3 \dots$).
5. **Evaluator Observation:** Scoring values update immediately and reflect true IR mathematical formulas.

---

### Step 8: Instant Content Preview & Multi-Threading Verification

**Objective:** Verify non-blocking UI thread execution and instant snippet extraction.

1. In the search results table, click through various rows.
2. **Evaluator Observation:**
   - The preview pane instantly updates with the extracted text content of the selected file.
   - Window can be resized, dragged, and interacted with while background workers run, confirming 60 fps UI responsiveness.

---

### Step 9: Binary Serialization, Persistence & CRC-32 Validation

**Objective:** Demonstrate disk persistence, compact binary layout, and corruption recovery.

1. Note the current indexed document count.
2. Close the application.
3. Inspect `search.idx` in the working directory:
   - Size is compact ($\sim 1.1$ MB for 10,000 documents).
4. Re-launch `UniversalSmartSearchEngine`.
5. **Evaluator Observation:**
   - Application loads the index from `search.idx` in $< 10$ milliseconds.
   - CRC-32 checksum is verified; document catalog and inverted index are fully restored without rescanning the disk.

---

### Step 10: Adversarial Fault Injection & Tombstone Compaction

**Objective:** Prove resilience against corrupted files, malformed syntax, and garbage collection.

1. Enter an unclosed parenthesis query in the search bar:
   ```text
   (dijkstra AND algorithm OR (unclosed
   ```
2. **Evaluator Observation:** The search bar indicates a syntax error with exact character offset; the application does not crash.
3. Test compaction:
   - Delete a file from the indexed folder.
   - Click **Reindex**; file is detected as removed and tombstoned (`META_FLAG_TOMBSTONE`).
   - Click **Tools -> Compact Index**; tombstoned records are purged and DocIDs remapped cleanly.
