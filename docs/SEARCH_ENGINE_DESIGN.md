# Search Engine Pipeline Design (SEARCH_ENGINE_DESIGN)
## Project: Universal Smart Search Engine
### Release 1 — Local Universal File Search Engine
**Document Version:** 1.0.0  
**Target Platform:** Windows Desktop (C++17 / MSVC 2022 x64 / Qt 6 Widgets / CMake)  
**Academic Context:** University Data Structures & Algorithms Coursework  
**Status:** Approved for Architectural Baseline  

---

## 1. Search Pipeline Architecture

The Search Engine pipeline converts user search strings into ranked, relevant file results through a 16-stage deterministic processing flow:

```
[1. User Query Input]
        │
        ▼
[2. Query Normalization & Trimming]
        │
        ├─────────────────────────────► [5. Trie Autocomplete Lookup (< 5 ms)]
        │                                       │
        ▼                                       ▼
[3. Lexical Tokenization]               [Top 5 Keystroke Suggestions]
        │
        ▼
[4. Stop-Word Filtering (Content Only)]
        │
        ▼
[6. Spell Correction (If Zero Hits in Index)]
        │
        ▼
[7. Boolean Query Parsing (Shunting-Yard Stack)]
        │
        ▼
[8. Posting List Retrieval (CustomHashTable Lookup)]
        │
        ▼
[9. AND Intersection] ──► [10. OR Union] ──► [11. NOT Difference]
        │
        ▼
[Candidate Document Set (DocIDs)]
        │
        ▼
[12. Unified TF-IDF Relevance Scoring (Filename + Body Weights)]
        │
        ▼
[13. Custom Max Heap Priority Ranking]
        │
        ▼
[14. Top-K Result Extraction (K = 50)]
        │
        ▼
[15. Snippet Generation & Context Extraction]
        │
        ▼
[16. Keyword Highlighting in Preview Pane]
```

---

## 2. Pipeline Stages Detailed Specification

### Stage 1: Query Input
Captures raw text from the Qt search input widget (`QLineEdit`). Can contain keywords, Boolean operators (`AND`, `OR`, `NOT`), parentheses `()`, and metadata filter prefixes (e.g., `ext:pdf`, `type:doc`).

### Stage 2: Query Normalization
1. Trims leading and trailing whitespace.
2. Collapses consecutive internal spaces into a single space.
3. Normalizes Unicode characters and converts all non-operator terms to lowercase.
4. Identifies case-insensitive operators: `and`, `or`, `not` $\to$ capitalized `AND`, `OR`, `NOT`.

### Stage 3: Lexical Tokenization
Splits normalized query string into discrete tokens:
- **Operator Tokens:** `TOKEN_AND`, `TOKEN_OR`, `TOKEN_NOT`, `TOKEN_LPAREN`, `TOKEN_RPAREN`.
- **Term Tokens:** Words delimited by spaces or punctuation (`TOKEN_TERM`).
- **Implicit Conjunction Insertion:** If two consecutive term tokens appear without an intervening operator (e.g., `binary search tree`), the tokenizer automatically inserts an implicit `AND` operator (`binary AND search AND tree`).

### Stage 4: Stop-Word Handling
- Standard grammatical stop words (e.g., *the, is, at, which, on*) are filtered out during content indexing to reduce index bloat.
- **Query Rule:** In search queries, single stop words (e.g., searching `the`) are preserved if the entire query consists of stop words; otherwise, non-operator stop words that do not exist in the Inverted Index vocabulary are bypassed with an informational status note.

### Stage 5: Trie Autocomplete Lookup & Multilingual Handling
- Triggered asynchronously on every keystroke in the search box (minimum 2 characters).
- **ASCII Alphanumeric Scope:** The `CustomTrie` manages Latin alphanumeric prefixes (`a-z`, `0-9`). It traverses to the prefix node in $O(P)$ time and collects up to 5 highest-frequency completions in under $5\ \text{ms}$.
- **Multilingual / Non-ASCII Guarantee:** If a user query contains non-ASCII characters (e.g., Urdu `اردو`, Arabic, Cyrillic, CJK), the engine does not discard or drop the text. It safely recognizes the multi-byte sequence, skips the ASCII Trie, and evaluates the query directly against the `CustomHashTable` (where all raw UTF-8 tokens are 100% indexed). Autocomplete for non-ASCII prefixes falls back to a linear scan of recent vocabulary or returns an empty suggestion list without crashing.

### Stage 6: Spell Correction ("Did You Mean?")
- Activated if an executed query produces zero matching documents in the Inverted Index.
- Evaluates misspelled Latin alphanumeric tokens against the `CustomTrie` vocabulary using Levenshtein distance ($\le 2$ edits):
  $$D(i, j) = \min \begin{cases} D(i-1, j) + 1 & \text{(Deletion)} \\ D(i, j-1) + 1 & \text{(Insertion)} \\ D(i-1, j-1) + \text{cost} & \text{(Substitution)} \end{cases}$$
- Ranks candidate corrections by lowest edit distance and highest vocabulary search frequency.
- Generates a clickable recommendation in the UI: *"Did you mean: **algorithm**?"*.
- **Language Boundary:** Spell correction is explicitly supported for Latin alphanumeric terms. Non-Latin tokens are searched verbatim in the Inverted Index without phonetic/edit mutation.

### Stage 7: Boolean Query Parsing (Shunting-Yard Stack)
Converts infix expressions with arbitrary parentheses into postfix Reverse Polish Notation (RPN) using `CustomStack<Token>` and `CustomQueue<Token>`.

#### Grammar Rules & Precedence:
1. `(` `)` : Parentheses (Innermost evaluated first).
2. `NOT`   : Unary difference (Precedence 3, Right-to-Left associativity).
3. `AND`   : Binary intersection (Precedence 2, Left-to-Right associativity).
4. `OR`    : Binary union (Precedence 1, Left-to-Right associativity).

#### Mismatched Parentheses Guard:
If the operator stack contains unclosed `(` upon reaching end-of-query, or encounters an unexpected `)` without an opening counterpart, the parser immediately halts and returns a descriptive syntax error:
```
Syntax Error: Mismatched parentheses at column 14.
Query: "(data structures AND (algorithms)"
                           ^
```

### Stage 8: Posting List Retrieval
For each term operand in the postfix queue, `BooleanSearchEngine` performs an $O(1)$ lookup in `InvertedIndex`'s `CustomHashTable`.
- If term exists: Pushes pointer to `CustomPostingList` onto evaluation stack.
- If term does not exist: Pushes an empty `CustomPostingList` onto evaluation stack.

### Stages 9–11: Boolean Operations Execution
The postfix stream is evaluated using `CustomStack<CustomPostingList*>`:

```
Algorithm EvaluateRPN(rpnQueue, invertedIndex, universeList):
    evalStack = CustomStack<CustomPostingList*>()

    while not rpnQueue.empty():
        token = rpnQueue.dequeue()

        if token.type == TOKEN_TERM:
            evalStack.push(invertedIndex.GetPostingList(token.value))
        else if token.type == TOKEN_AND:
            listB = evalStack.pop()
            listA = evalStack.pop()
            evalStack.push(CustomPostingList::Intersect(listA, listB))
        else if token.type == TOKEN_OR:
            listB = evalStack.pop()
            listA = evalStack.pop()
            evalStack.push(CustomPostingList::Union(listA, listB))
        else if token.type == TOKEN_NOT:
            listA = evalStack.pop()
            evalStack.push(CustomPostingList::Difference(universeList, listA))

    return evalStack.pop() // Final candidate set
```

- **Set Operation Complexities:**
  - `Intersect(A, B)`: $O(L_A + L_B)$ using two-pointer sweep on sorted `DocID`s.
  - `Union(A, B)`: $O(L_A + L_B)$ using two-pointer merge.
  - `Difference(Universe, A)`: $O(N + L_A)$ subtracting $A$'s postings from active catalog documents.

---

## 3. Relevance Ranking Engine (TF-IDF & Heap Selection)

### 3.1 Unification of Metadata and Content Scoring
A key challenge is ranking results fairly when some documents match by filename (e.g., an image named `dijkstra_graph.png`) while others match by deep body text (e.g., a PDF mentioning "dijkstra" 40 times).

To achieve balanced, intuitive ranking, the system computes a composite field-weighted score utilizing the independent frequency counters stored in `PostingNode` (`nameTermFreq` and `bodyTermFreq`):

$$\text{Score}(d, Q) = \left[ \sum_{t \in Q \cap d} \left( \left(TF_{body}(t, d) \times W_{body}\right) + \left(TF_{name}(t, d) \times W_{name}\right) \right) \times \ln\left(1 + \frac{N}{DF_t}\right) \right] \times \text{ModBonus}(d)$$

#### Component Breakdown:
1. **$TF_{body}(t, d)$:** Stored in `posting.bodyTermFreq`. Log-normalized to prevent long documents from dominating results:
   $$TF_{body}(t, d) = \begin{cases} 1 + \ln(posting.bodyTermFreq) & \text{if } posting.bodyTermFreq > 0 \\ 0 & \text{otherwise} \end{cases}$$
2. **$W_{body} = 1.0$:** Baseline weight for body text occurrences.
3. **$TF_{name}(t, d)$:** Stored in `posting.nameTermFreq`. Evaluated directly with linear weighting: $TF_{name}(t, d) = posting.nameTermFreq$.
4. **$W_{name} = 5.0$:** Elevates filename matches so a file explicitly named after the query term ranks above documents that merely mention the term in passing.
5. **$\text{IDF}(t) = \ln\left(1 + \frac{N}{DF_t}\right)$:** Inverse document frequency, penalizing common terms and rewarding rare, specific keywords.
6. **$\text{ModBonus}(d)$:** Recency multiplier ($1.00 \le \text{ModBonus} \le 1.10$) giving slight preference to recently modified files.

> [!IMPORTANT]
> **Design Decision Requiring Approval:** The field weight constants ($W_{name} = 5.0$, $W_{body} = 1.0$) and recency bonus formulation are recommended engineering defaults. They are explicitly flagged as an Open Architecture Decision for user approval.

### 3.2 Top-$K$ Heap Selection Strategies

To extract the Top-$K$ results ($K = 50$) from $M$ matching candidates, two heap-based strategies are evaluated:

#### Strategy A: Unbounded Max-Heap ($O(M)$ Build + $K$ Extractions)
- Inserts all $M$ candidates into an array and calls Floyd's linear `buildHeap()` in $O(M)$ time.
- Extracts the top $K$ items via $K$ successive `extractMax()` operations in $K \times O(\log M)$ time.
- **Time Complexity:** $O(M + K \log M)$.
- **Auxiliary Space:** $O(M)$ heap nodes. Requires allocating memory proportional to candidate set size $M$.

#### Strategy B: Bounded Min-Heap of Size $K$ ($O(M \log K)$ Online Filtering)
- Allocates a fixed-capacity heap storing exactly $K$ nodes: $O(K)$ space.
- Streams through the $M$ candidates:
  - If heap size $< K$: Insert candidate in $O(\log K)$.
  - If heap size $== K$: Compare candidate score with the min-heap root (the current $K$-th best score):
    - If score $\le$ root: Discard immediately in $O(1)$.
    - If score $>$ root: Replace root and call `heapifyDown()` in $O(\log K)$.
- **Time Complexity:** $O(M \log K)$ worst-case. In practice, as the score threshold increases, the majority of candidates are rejected in $O(1)$, resulting in near-linear typical runtime.
- **Auxiliary Space:** Strictly bounded $O(K)$ heap nodes (e.g., exactly 50 nodes $\approx 1.2\ \text{KB}$ regardless of whether $M = 1,000$ or $100,000$).

#### Strategy Comparison & Architectural Selection:
- **Primary Production Choice:** **Strategy B (Bounded Min-Heap of size $K$)** is selected for the runtime search pipeline because of its deterministic, minimal memory footprint ($O(K)$ space).
- **Secondary Batch Choice:** Strategy A is supported when batch ranking pre-allocated candidate arrays.
- **Tie-Breaking Determinism:**
  1. Higher `score` wins.
  2. If scores equal: More recent `lastModifiedTime` wins.
  3. If timestamps equal: Smaller `docId` wins.

---

## 4. Snippet Generation & Keyword Highlighting

### 4.1 On-Demand Context Snippet Extraction
To keep memory compact, snippets are generated **on demand** only for the Top-$K$ results displayed on screen:
1. The file path is retrieved from `DocumentCatalog[docId]`.
2. If file is a text/code document, `ContentExtractor` reads the first occurrence of any query keyword.
3. A 160-character sliding context window is extracted:
   $$\text{Window} = [\text{pos} - 60\ \text{chars}, \ \text{pos} + 100\ \text{chars}]$$
4. Word boundaries are adjusted so words are not cut in half, prepending and appending ellipsis (`...`).

### 4.2 Preview Pane Highlighting
When a user selects a result in the GUI:
1. The file text is loaded into a read-only `QPlainTextEdit` or `QTextBrowser`.
2. A syntax highlighter traverses the document text finding all occurrences of active query terms.
3. Matching ranges receive a yellow background highlight (`QTextCharFormat::setBackground(QColor(255, 235, 59))`).
4. The viewer automatically scrolls to the first highlighted match.
