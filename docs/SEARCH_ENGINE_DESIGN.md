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

## 3. Relevance Ranking Engine (TF-IDF, BM25 & Heap Selection)

### 3.1 Field-Aware Relevance Models
The ranking subsystem implements two production relevance algorithms:

#### 1. Field-Weighted BM25 (Default)
$$\text{Score}_{\text{BM25}}(d, Q) = \sum_{t \in Q \cap d} \text{IDF}_{\text{BM25}}(t) \cdot \left[ W_{\text{name}} \cdot S_{\text{BM25}}(t, d_{\text{name}}) + W_{\text{body}} \cdot S_{\text{BM25}}(t, d_{\text{body}}) \right]$$

Where:
- $\text{IDF}_{\text{BM25}}(t) = \ln\left(1 + \frac{N - DF_t + 0.5}{DF_t + 0.5}\right)$.
- Saturation: $S_{\text{BM25}}(t, d_{\text{field}}) = \frac{\text{TF}(t, d_{\text{field}}) \cdot (k_1 + 1)}{\text{TF}(t, d_{\text{field}}) + k_1 \cdot \left(1 - b + b \cdot \frac{|d_{\text{field}}|}{\text{avgdl}_{\text{field}}}\right)}$.
- Parameters: $k_1 = 1.2$, $b = 0.75$, $W_{\text{name}} = 5.0$, $W_{\text{body}} = 1.0$.

#### 2. Field-Weighted TF-IDF
$$\text{Score}_{\text{TF-IDF}}(d, Q) = \sum_{t \in Q \cap d} \left[ W_{\text{name}} \cdot \text{TF}(t, d_{\text{name}}) + W_{\text{body}} \cdot \text{TF}(t, d_{\text{body}}) \right] \cdot \text{IDF}(t)$$

### 3.2 Top-$K$ Heap Selection
To extract the Top-$K$ results efficiently without sorting all $M$ matching candidates:
- **Bounded Min-Heap of Size $K$:** Uses `CustomMinHeap` with $O(M \log K)$ worst-case time and $O(K)$ space.
- **Deterministic Tie-Breaking:**
  1. Higher relevance score first.
  2. If scores are equal: Smaller `docId` first.

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
