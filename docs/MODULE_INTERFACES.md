# Module Interfaces & Contract Specifications (MODULE_INTERFACES)
## Project: Universal Smart Search Engine
### Release 1 — Local Universal File Search Engine
**Document Version:** 1.0.0  
**Target Platform:** Windows Desktop (C++17 / MSVC 2022 x64 / Qt 6 Widgets / CMake)  
**Academic Context:** University Data Structures & Algorithms Coursework  
**Status:** Approved for Architectural Baseline  

---

## 1. Architectural Interface Rules

1. **Design Specifications Only:** This document provides abstract C++17 class blueprints, method signatures, return types, and ownership annotations. It contains zero implementation code.
2. **Strict Single Ownership:** Explicit ownership annotations (`[OWNS]`, `[BORROWS]`, `[VALUE_RETURN]`) govern every pointer and reference crossing module boundaries.
3. **No Circular Dependencies:** Dependencies strictly flow downward through the layered architecture. High-level modules call lower-level modules; lower-level modules communicate upward only via callbacks, abstract observer interfaces, or Qt signals/slots.
4. **Error Reporting Contracts:** Operations that can fail return explicit `Result<T>` or `std::optional<T>` types, or propagate typed error codes. No unhandled exceptions cross thread boundaries.

---

## 2. Module Specifications

---

### 2.1 FileScanner
- **Responsibility:** Traverses directory trees using `CustomQueue` BFS, discovers files, skips exclusions, and handles access errors.
- **Inputs:** List of root directory paths, cancellation token.
- **Outputs:** Emits paths of discovered files to `IndexingCoordinator`.
- **Dependencies:** `CustomQueue`, `FileSystemUtils`, `Logger`.
- **Ownership:** Owns internal directory queue. Discovered paths emitted by value.
- **Error Strategy:** Uses `std::error_code` with filesystem calls; logs warnings and skips unreadable directories.

#### Public Operations Interface Specification:
```cpp
class FileScanner {
public:
    explicit FileScanner(std::shared_ptr<Logger> logger);
    ~FileScanner();

    // Configuration
    void setRootDirectories(const std::vector<std::string>& roots);
    void setExclusionPatterns(const std::vector<std::string>& patterns);
    void setMaxDepth(uint32_t depth);

    // Traversal execution (intended for worker thread)
    void startScan(std::function<void(const std::string& filePath)> onFileDiscovered,
                   std::function<void(uint64_t scanned, uint64_t totalEstimate)> onProgress,
                   std::atomic<bool>& cancelRequested);
};
```

---

### 2.2 FileMetadataManager
- **Responsibility:** Queries filesystem attributes for discovered paths, assigns category tags, and constructs `FileMetadata` records.
- **Inputs:** Discovered file path.
- **Outputs:** Populated `FileMetadata` value object.
- **Dependencies:** `std::filesystem`.
- **Ownership:** Yields `FileMetadata` by value to `DocumentCatalog`.
- **Error Strategy:** Missing timestamps default to epoch 0; inaccessible attributes set error flags.

#### Public Operations Interface Specification:
```cpp
class FileMetadataManager {
public:
    FileMetadataManager() = default;
    
    // Extracts standard OS metadata
    bool extractMetadata(const std::string& path, FileMetadata& outMetadata);
    
    // Categorization based on extension
    FileCategory classifyExtension(const std::string& extension) const;
};
```

---

### 2.3 ContentExtractor
- **Responsibility:** Coordinates format-specific file readers to yield decoded UTF-8 text streams.
- **Inputs:** File path, `FileCategory`.
- **Outputs:** UTF-8 plain text buffer and `ExtractionStatus`.
- **Dependencies:** Text decoders, logging.
- **Ownership:** Produces text buffers returned by value or move semantics.
- **Error Strategy:** Catches read errors, binary corruption, and OOM risks; returns `ExtractionStatus::Failed` or `Truncated`.

#### Public Operations Interface Specification:
```cpp
class ContentExtractor {
public:
    ContentExtractor();
    ~ContentExtractor();

    void registerExtractor(std::unique_ptr<IContentExtractor> extractor);

    // Primary extraction entry point (enforces 50 MB safety cap)
    ExtractionResult extract(const std::string& path, 
                            FileCategory category, 
                            std::string& outText, 
                            size_t maxBytes = 50 * 1024 * 1024);
};
```

---

### 2.4 TextProcessor
- **Responsibility:** Lexical tokenization, lowercase normalization, punctuation removal, length filtering, and stop-word filtering.
- **Inputs:** Raw text buffer, field source flag (`FIELD_FILENAME` vs `FIELD_CONTENT`).
- **Outputs:** List/stream of normalized token strings.
- **Dependencies:** `CustomHashTable` (for $O(1)$ stop-word set).
- **Ownership:** Borrowed string view inputs; returns token collections by value.
- **Error Strategy:** Gracefully ignores unparseable Unicode sequences.

#### Public Operations Interface Specification:
```cpp
class TextProcessor {
public:
    explicit TextProcessor(const std::string& stopWordsPath);
    
    // Tokenizes and filters stop-words
    std::vector<std::string> processText(const std::string& rawText, bool isFilename);
    
    // Single term normalization (used by QueryParser)
    std::string normalizeTerm(const std::string& term) const;
    
    bool isStopWord(const std::string& term) const;
};
```

---

### 2.5 DocumentCatalog
- **Responsibility:** Central registry mapping sequential 32-bit `DocID`s to `FileMetadata` records and managing file path lookups.
- **Inputs:** `FileMetadata` records, file paths.
- **Outputs:** `DocID` lookups in $O(1)$, metadata access in $O(1)$.
- **Dependencies:** `CustomDynamicArray<FileMetadata>`, `CustomHashTable<std::string, uint32_t>`.
- **Ownership:** `[OWNS]` all `FileMetadata` structs and string pool storage.
- **Error Strategy:** Invalid `DocID` returns `nullptr` or throws `std::out_of_range`.

#### Public Operations Interface Specification:
```cpp
class DocumentCatalog {
public:
    DocumentCatalog();
    ~DocumentCatalog();

    // Ingestion
    uint32_t registerDocument(const FileMetadata& metadata, const std::string& path, const std::string& filename);
    
    // Accessors
    const FileMetadata* getMetadata(uint32_t docId) const;
    std::string getPath(uint32_t docId) const;
    std::string getFilename(uint32_t docId) const;
    
    // Reverse lookup
    uint32_t findDocIdByPath(const std::string& canonicalPath) const;

    // Maintenance
    void markDeleted(uint32_t docId);
    bool isActive(uint32_t docId) const;
    
    size_t totalCount() const;
    size_t activeCount() const;
    void clear();
};
```

---

### 2.6 InvertedIndex
- **Responsibility:** Maps vocabulary terms to their respective `CustomPostingList` instances via `CustomHashTable`.
- **Inputs:** Normalized terms, `DocID`, field bitmask.
- **Outputs:** Pointers to `CustomPostingList` instances.
- **Dependencies:** `CustomHashTable<std::string, CustomPostingList*>`, `CustomPostingList`.
- **Ownership:** `[OWNS]` all `CustomPostingList` instances. Returns borrowed pointers `[BORROWS]`.
- **Error Strategy:** Lookup of unknown term returns `nullptr`.

#### Public Operations Interface Specification:
```cpp
class InvertedIndex {
public:
    InvertedIndex();
    ~InvertedIndex();

    // Ingestion
    void addTerm(const std::string& term, uint32_t docId, uint8_t fieldMask);

    // Retrieval
    CustomPostingList* getPostingList(const std::string& term) const;
    uint32_t getDocumentFrequency(const std::string& term) const;

    size_t vocabularySize() const;
    uint64_t totalPostingsCount() const;
    void clear();
};
```

---

### 2.7 PostingList (`CustomPostingList`)
- **Responsibility:** Maintains sorted singly linked chain of 24-byte `PostingNode` records; implements set intersection, union, and difference.
- **Inputs:** `DocID`, `nameTermFreq`, `bodyTermFreq`, `fieldMask`.
- **Outputs:** Linked posting nodes, merged result lists.
- **Dependencies:** `CustomSinglyLinkedList`.
- **Ownership:** `[OWNS]` internal chain of `PostingNode`s. Set operations return new instances by value/move.

#### Public Operations Interface Specification:
```cpp
struct PostingNode {
    uint32_t docId;        // 4 bytes: Document identifier (1 .. N)
    uint16_t bodyTermFreq; // 2 bytes: Occurrences in body content
    uint16_t nameTermFreq; // 2 bytes: Occurrences in base filename
    uint8_t  fieldMask;    // 1 byte:  Bitmask (0x01 = Name, 0x02 = Content)
    uint8_t  padding[7];   // 7 bytes: Compiler alignment padding for 8-byte pointer
    PostingNode* next;     // 8 bytes: 64-bit address (Total size = 24 bytes)
};

class CustomPostingList {
public:
    CustomPostingList();
    ~CustomPostingList();

    // Rule of Five
    CustomPostingList(const CustomPostingList& other);
    CustomPostingList(CustomPostingList&& other) noexcept;
    CustomPostingList& operator=(const CustomPostingList& other);
    CustomPostingList& operator=(CustomPostingList&& other) noexcept;

    void appendNode(uint32_t docId, uint16_t nameFreq, uint16_t bodyFreq, uint8_t fieldMask);
    
    // Set operations (Linear two-pointer algorithms)
    static CustomPostingList intersect(const CustomPostingList* a, const CustomPostingList* b);
    static CustomPostingList unionLists(const CustomPostingList* a, const CustomPostingList* b);
    static CustomPostingList difference(const CustomPostingList* universe, const CustomPostingList* exclude);

    const PostingNode* head() const;
    uint32_t documentFrequency() const;
    bool empty() const;
};
```

---

### 2.8 TrieAutocomplete
- **Responsibility:** Prefix search tree providing keystroke-level autocomplete suggestions for Latin alphanumeric prefixes with safe fallback for non-ASCII/multilingual tokens.
- **Inputs:** Vocabulary terms during index build; user prefix during search.
- **Outputs:** Array of up to 5 suggested words with occurrence frequencies.
- **Dependencies:** `CustomTrie`, `CustomMaxHeap` / `CustomMinHeap`.
- **Ownership:** `[OWNS]` internal `CustomTrie`.

#### Public Operations Interface Specification:
```cpp
class TrieAutocomplete {
public:
    TrieAutocomplete();
    ~TrieAutocomplete();

    void insertWord(const std::string& word, uint32_t initialFrequency = 1);
    void incrementSearchFrequency(const std::string& word);
    
    // Returns top 5 completions ordered by frequency (with non-ASCII fallback)
    std::vector<std::string> getSuggestions(const std::string& prefix, size_t maxSuggestions = 5) const;
    
    void clear();
};
```

---

### 2.9 SpellChecker
- **Responsibility:** Levenshtein distance $\le 2$ vocabulary exploration to generate "Did you mean?" suggestions on zero-hit queries.
- **Inputs:** Misspelled query term.
- **Outputs:** Best matching candidate words.
- **Dependencies:** `TrieAutocomplete`, `InvertedIndex`.

#### Public Operations Interface Specification:
```cpp
class SpellChecker {
public:
    explicit SpellChecker(const TrieAutocomplete* trie);
    
    std::vector<std::string> suggestCorrections(const std::string& misspelledWord, size_t maxEdits = 2) const;
};
```

---

### 2.10 QueryParser
- **Responsibility:** Lexical query parsing and Shunting-Yard conversion into Reverse Polish Notation.
- **Inputs:** Raw query string.
- **Outputs:** `CustomQueue<QueryToken>` in postfix order or syntax error descriptor.
- **Dependencies:** `CustomStack<Token>`, `CustomQueue<Token>`.

#### Public Operations Interface Specification:
```cpp
class QueryParser {
public:
    QueryParser() = default;

    ParseResult parse(const std::string& queryString);
};
```

---

### 2.11 BooleanSearchEngine
- **Responsibility:** Evaluates postfix Boolean token streams via posting list set operations.
- **Inputs:** Postfix queue from `QueryParser`.
- **Outputs:** Evaluated `CustomPostingList` of candidate documents.
- **Dependencies:** `CustomStack`, `InvertedIndex`, `DocumentCatalog`.

#### Public Operations Interface Specification:
```cpp
class BooleanSearchEngine {
public:
    BooleanSearchEngine(const InvertedIndex* index, const DocumentCatalog* catalog);

    CustomPostingList evaluate(CustomQueue<QueryToken>& postfixQueue);
};
```

---

### 2.12 RankingEngine
- **Responsibility:** Computes composite TF-IDF relevance scores for matching candidates and extracts Top-$K$ results via `CustomMaxHeap`.
- **Inputs:** Candidate `CustomPostingList`, parsed query terms, target integer $K$.
- **Outputs:** Array of Top-$K$ `SearchResultItem` DTOs.
- **Dependencies:** `CustomMaxHeap`, `InvertedIndex`, `DocumentCatalog`.

#### Public Operations Interface Specification:
```cpp
class RankingEngine {
public:
    RankingEngine(const InvertedIndex* index, const DocumentCatalog* catalog);

    std::vector<SearchResultItem> rank(const CustomPostingList& candidates, 
                                       const std::vector<std::string>& queryTerms, 
                                       size_t topK = 50);
};
```

---

### 2.13 MaxHeap (`CustomMaxHeap<T>`)
- **Responsibility:** Binary max-heap supporting linear $O(M)$ `buildHeap` and $O(\log M)$ `extractMax`.
- **Inputs:** Unsorted array of scored items.
- **Outputs:** Descending extraction of highest scoring items.
- **Dependencies:** `CustomDynamicArray`.

#### Public Operations Interface Specification:
```cpp
template <typename T>
class CustomMaxHeap {
public:
    CustomMaxHeap();
    ~CustomMaxHeap();

    void insert(const T& item);
    T extractMax();
    const T& peekMax() const;
    
    void buildHeap(const CustomDynamicArray<T>& unsortedItems);
    
    size_t size() const;
    bool empty() const;
};
```

---

### 2.14 IndexStorage
- **Responsibility:** Atomic binary serialization and deserialization of `DocumentCatalog` and `InvertedIndex` with CRC32 integrity checks.
- **Inputs:** In-memory index structures on save; file path on load.
- **Outputs:** Binary file on disk; restored memory structures.
- **Dependencies:** `DocumentCatalog`, `InvertedIndex`, `TrieAutocomplete`, `FileSystemUtils`.

#### Public Operations Interface Specification:
```cpp
class IndexStorage {
public:
    IndexStorage() = default;

    bool saveIndex(const std::string& filePath, 
                   const DocumentCatalog& catalog, 
                   const InvertedIndex& index);

    bool loadIndex(const std::string& filePath, 
                   DocumentCatalog& outCatalog, 
                   InvertedIndex& outIndex, 
                   TrieAutocomplete& outTrie);
};
```

---

### 2.15 SearchHistory
- **Responsibility:** Maintains persistent circular history of executed user queries.
- **Inputs:** Search query, latency, result count.
- **Outputs:** List of past search records for UI dropdown.

#### Public Operations Interface Specification:
```cpp
class SearchHistory {
public:
    explicit SearchHistory(const std::string& persistencePath, size_t maxCapacity = 100);

    void recordQuery(const std::string& query, size_t resultCount, double latencyMs);
    std::vector<HistoryRecord> getRecentQueries() const;
    void clear();
};
```

---

### 2.16 SearchController
- **Responsibility:** Application service mediator connecting the presentation layer to the core search engine and coordinating background workers.
- **Inputs:** High-level search commands and filter parameters from GUI.
- **Outputs:** Transmits UI DTOs and notifications via Qt signals.

#### Public Operations Interface Specification (Qt Signals / Slots):
```cpp
class SearchController : public QObject {
    Q_OBJECT
public:
    explicit SearchController(QObject* parent = nullptr);
    ~SearchController();

public slots:
    void executeSearch(const QString& queryText, const SearchFilters& filters);
    void requestAutocomplete(const QString& prefix);
    void cancelCurrentSearch();

signals:
    void searchCompleted(const std::vector<SearchResultDTO>& results, double latencyMs);
    void autocompleteReady(const QStringList& suggestions);
    void searchError(const QString& errorMessage);
};
```

---

### 2.17 StatisticsManager
- **Responsibility:** Gathers telemetry on memory usage, vocabulary size, postings count, and file category distribution.
- **Inputs:** Internal index references.
- **Outputs:** Formatted `EngineStatisticsDTO`.

#### Public Operations Interface Specification:
```cpp
class StatisticsManager {
public:
    StatisticsManager(const DocumentCatalog* catalog, const InvertedIndex* index);

    EngineStatisticsDTO getStatistics() const;
};
```

---

### 2.18 Qt Presentation Layer (`MainWindow`)
- **Responsibility:** Hosts Qt 6 Widgets, captures user input, displays results, and renders preview widgets.
- **Inputs:** User mouse and keyboard events.
- **Outputs:** Screen presentation.
- **Dependencies:** Qt 6 (`QtWidgets`, `QtGui`, `QtCore`), `SearchController`.
- **Communication:** Connects strictly to `SearchController` and `IndexingWorker` via Qt signals and slots.
