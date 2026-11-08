# Data Structures & Algorithms Specification (DSA_DESIGN)
## Project: Universal Smart Search Engine
### Release 1 — Local Universal File Search Engine
**Document Version:** 1.0.0  
**Target Platform:** Windows Desktop (C++17 / MSVC 2022 x64 / Qt 6 Widgets / CMake)  
**Academic Context:** University Data Structures & Algorithms Coursework  
**Status:** Approved for Implementation Baseline  

---

## 1. Custom Singly Linked List (`CustomSinglyLinkedList<T>`)

### 1.1 Architectural Purpose & Role in System
The `CustomSinglyLinkedList<T>` is a generic, dynamic node-based sequential data structure engineered to manage variable-length sequences without contiguous memory reallocation. In this project, it serves as the algorithmic foundation for:
1. **Collision Chains in the Custom Hash Table:** Resolving hash collisions via separate chaining.
2. **Posting List Core Chain:** Providing the structural backbone for the document occurrence lists associated with each vocabulary term.

### 1.2 Node Structure & Class Invariants
Each node encapsulates a payload of type `T` and a single forward pointer to the subsequent node:

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ Node 1       │     │ Node 2       │     │ Node 3       │
│ ┌──────────┐ │     │ ┌──────────┐ │     │ ┌──────────┐ │
│ │ Data: T  │ │     │ │ Data: T  │ │     │ │ Data: T  │ │
│ ├──────────┤ │     │ ├──────────┤ │     │ ├──────────┤ │
│ │ Next: ───┼─┼────►│ │ Next: ───┼─┼────►│ │ Next: ───┼─┼────► nullptr
│ └──────────┘ │     │ └──────────┘ │     │ └──────────┘ │
└──────────────┘     └──────────────┘     └──────────────┘
  ▲                                         ▲
  │                                         │
Head Pointer                              Tail Pointer
```

#### Node Specification (Interface):
```
Type: ListNode<T>
Fields:
    data: T
    next: ListNode<T>* (default: nullptr)
```

#### Container Invariants:
- If `size == 0`, then `head == nullptr` and `tail == nullptr`.
- If `size == 1`, then `head == tail` and `head->next == nullptr`.
- If `size > 1`, `tail->next == nullptr` and traversing from `head` through `next` pointers yields exactly `size` nodes before encountering `nullptr`.

### 1.3 Core Operations & Algorithmic Design

#### 1.3.1 Insertion at Tail (`push_back(const T& value)`)
```
Algorithm PushBack(list, value):
    newNode = allocate ListNode<T>(value)
    if list.tail is nullptr:
        list.head = newNode
        list.tail = newNode
    else:
        list.tail.next = newNode
        list.tail = newNode
    list.size = list.size + 1
```
- **Complexity:** $O(1)$ time, $O(1)$ auxiliary space.

#### 1.3.2 Insertion at Head (`push_front(const T& value)`)
```
Algorithm PushFront(list, value):
    newNode = allocate ListNode<T>(value)
    newNode.next = list.head
    list.head = newNode
    if list.tail is nullptr:
        list.tail = newNode
    list.size = list.size + 1
```
- **Complexity:** $O(1)$ time, $O(1)$ auxiliary space.

#### 1.3.3 Removal by Value / Predicate (`remove_if(predicate)`)
```
Algorithm RemoveIf(list, predicate):
    current = list.head
    prev = nullptr
    while current is not nullptr:
        if predicate(current.data):
            toDelete = current
            if prev is nullptr:
                list.head = current.next
            else:
                prev.next = current.next
            if toDelete == list.tail:
                list.tail = prev
            current = current.next
            deallocate toDelete
            list.size = list.size - 1
            return true
        else:
            prev = current
            current = current.next
    return false
```
- **Complexity:** $O(N)$ time, $O(1)$ auxiliary space.

#### 1.3.4 Traversal & Search
Traversal iterates linearly from `head` to `nullptr`. Linear search checks each node against a target key or predicate.
- **Complexity:** $O(N)$ time.

### 1.4 Memory Management & Edge Cases
- **Destructor:** Iteratively traverses and deallocates every node until `head == nullptr`. Must never use recursive deallocation to avoid call-stack overflow on lists with $> 100,000$ nodes.
- **Rule of Five Compliance:** Custom copy constructor (deep clone), copy assignment (copy-and-swap idiom), move constructor ($O(1)$ pointer steal), and move assignment.
- **Edge Cases Handled:**
  - Deletion on empty list: Graceful no-op.
  - Deletion of the sole element (`head == tail`): Head and tail both reset to `nullptr`, `size` decrements to `0`.
  - Deletion of the current tail node: Tail pointer successfully updated to previous node.

---

## 2. Custom Dynamic Array (`CustomDynamicArray<T>`)

### 2.1 Architectural Purpose & Role in System
The `CustomDynamicArray<T>` is a contiguous, heap-allocated dynamic array that expands automatically as elements are added. It provides $O(1)$ random access, high CPU cache locality, and amortized $O(1)$ appends. In this project, it serves as the storage vehicle for:
1. **DocumentCatalog:** Storing `FileMetadata` records indexed directly by sequential `DocID`.
2. **CustomMaxHeap:** Serving as the underlying contiguous array for the binary heap.
3. **SearchHistory:** Managing the bounded list of recent user queries.

### 2.2 Internal Memory Representation & Growth Policy
```
┌────────────────────────────────────────────────────────────────────────┐
│                        Heap Memory Allocation Buffer                   │
├────────┬────────┬────────┬────────┬────────┬───────┬───────┬───────────┤
│ Slot 0 │ Slot 1 │ Slot 2 │ Slot 3 │ Slot 4 │ Empty │ Empty │   Empty   │
├────────┴────────┴────────┴────────┴────────┴───────┴───────┴───────────┤
│ <────────────── Size = 5 ────────────────> │                           │
│ <──────────────────────── Capacity = 8 ──────────────────────────────> │
└────────────────────────────────────────────────────────────────────────┘
  ▲
  │
m_data (Raw Pointer: T*)
```

#### Fields:
- `m_data`: Pointer to raw heap buffer `T*`.
- `m_size`: Number of currently constructed elements (`size_t`).
- `m_capacity`: Total allocated capacity (`size_t`).

#### Geometric Expansion Formula:
$$\text{NewCapacity} = \begin{cases} 8 & \text{if } \text{OldCapacity} = 0 \\ \lfloor 1.5 \times \text{OldCapacity} \rfloor & \text{if } \text{OldCapacity} > 0 \end{cases}$$
*Rationale for 1.5x Growth Factor:* A growth factor of $1.5$ (or $1.618$) allows memory deallocated in earlier resizing steps to be reused by future allocations, preventing heap fragmentation common to $2.0\times$ allocators under Windows heap managers.

### 2.3 Core Operations

#### 2.3.1 Amortized Push Back (`push_back(const T& val)`)
```
Algorithm PushBack(array, val):
    if array.m_size == array.m_capacity:
        newCapacity = (array.m_capacity == 0) ? 8 : (array.m_capacity + array.m_capacity / 2)
        Reserve(array, newCapacity)
    new (array.m_data + array.m_size) T(val)  // Placement new / copy construct
    array.m_size = array.m_size + 1
```
- **Amortized Time Complexity:** $O(1)$. Worst-case reallocation: $O(N)$.
- **Auxiliary Space:** $O(1)$ amortized.

#### 2.3.2 Random Access (`operator[](size_t index)`)
```
Algorithm At(array, index):
    if index >= array.m_size:
        raise OutOfBoundsException
    return array.m_data[index]
```
- **Time Complexity:** $O(1)$ deterministic.

#### 2.3.3 Memory Cleanup & Destruction
Destructor calls explicit destructors `array.m_data[i].~T()` for all $0 \le i < \text{size}$, then deallocates the raw byte buffer using `operator delete[]`.

---

## 3. Custom Hash Table — Separate Chaining (`CustomHashTable<K, V>`)

### 3.1 Architectural Purpose & Central Role
The `CustomHashTable<K, V>` is the central computational hub of the entire search engine. It implements an associative map from string keys to arbitrary values using separate chaining to handle hash collisions. It powers:
1. **The Inverted Index Vocabulary:** Mapping normalized word strings (`std::string`) to pointers of their corresponding `CustomPostingList`.
2. **Document Catalog Path Lookup:** Mapping canonical file paths to sequential `DocID`s for $O(1)$ duplicate checks.
3. **Stop-Word Filter:** Fast $O(1)$ lookup to ignore grammatical particles during tokenization.

```
Hash Table Array (Buckets: M = 2^k)
┌─────┐
│  0  │──► nullptr
├─────┤
│  1  │──► [ "algorithm" | PostingList* ] ──► [ "binary" | PostingList* ] ──► nullptr
├─────┤
│  2  │──► [ "compiler"  | PostingList* ] ──► nullptr
├─────┤
│ ... │
├─────┤
│ M-1 │──► [ "zebra"     | PostingList* ] ──► nullptr
└─────┘
```

### 3.2 Hash Function: FNV-1a (Fowler-Noll-Vo 1a)
To ensure rapid computation and near-uniform key dispersion across buckets, the engine utilizes the 64-bit FNV-1a hashing algorithm:

$$\text{hash} = \text{FNV\_OFFSET\_BASIS} \quad (14695981039346656037_{10})$$
$$\text{For each byte } b \text{ in key}: \quad \text{hash} = (\text{hash} \oplus b) \times \text{FNV\_PRIME} \quad (1099511628211_{10})$$

#### Bucket Calculation via Bitwise Masking:
The bucket count $M$ is maintained strictly as a power of two ($M = 2^k$, starting at $M = 16$). Calculating the bucket index uses bitwise AND rather than costly modulo division:
$$\text{BucketIndex} = \text{hash}(key) \ \& \ (M - 1)$$

### 3.3 Collision Resolution & Bucket Architecture
- Each bucket is a singly linked chain of `HashEntry<K, V>` nodes:
```
struct HashEntry {
    K key;
    V value;
    HashEntry* next;
};
```
- New entries in a bucket are pushed to the head of the collision chain in $O(1)$ after confirming key uniqueness.

### 3.4 Load Factor & Dynamic Rehashing
- **Load Factor Definition:** $\alpha = \frac{N}{M}$, where $N$ is the number of stored key-value pairs, and $M$ is the number of buckets.
- **Rehashing Threshold:** When $\alpha > 0.75$, the table automatically resizes:
  1. $M_{new} = 2 \times M_{old}$.
  2. A new bucket array of size $M_{new}$ is allocated.
  3. Every existing `HashEntry` is re-indexed via $\text{hash}(key) \ \& \ (M_{new} - 1)$ and spliced into the new bucket array.
  4. The old bucket array is deallocated without reallocating individual entry nodes (splicing nodes preserves pointer stability and avoids allocation overhead).
- **Time Complexity of Rehash:** $O(N)$ amortized.

### 3.5 Operations & Complexity Analysis

| Operation | Algorithm | Average Case | Worst Case (All keys collide) |
| :--- | :--- | :---: | :---: |
| **Insert** (`insert(key, val)`) | Compute hash $\to$ traverse chain $\to$ insert at head | $O(1)$ | $O(N)$ |
| **Search** (`find(key)`) | Compute hash $\to$ traverse chain comparing keys | $O(1)$ | $O(N)$ |
| **Delete** (`remove(key)`) | Compute hash $\to$ unlink entry from chain $\to$ free | $O(1)$ | $O(N)$ |
| **Update** (`operator[](key)`) | Locate entry or insert default constructed value | $O(1)$ | $O(N)$ |

---

## 4. Custom Posting Lists (`CustomPostingList`)

### 4.1 Data Structure Definition & Verified MSVC x64 Node Layout
A posting list represents the set of all documents in which a specific term appears. Each `CustomPostingList` is implemented as an ordered singly linked list of `PostingNode`s maintaining strict ascending `DocID` order.

#### 4.1.1 Memory Alignment & Padding Audit (MSVC 2022 x64)
In a 64-bit architecture (MSVC x64), pointers (`PostingNode* next`) require 8-byte alignment. Storing fields without accounting for padding leads to silent memory bloat or misaligned access penalties. Furthermore, relevance ranking requires independent access to term occurrences within the filename versus the body text without costly string re-evaluations.

The verified C++17 memory layout for `PostingNode` is:

```
┌────────────────────────────────────────────────────────────────────────┐
│                              PostingNode                               │
├───────────────────┬───────────────────┬──────────────────┬─────────────┤
│ uint32_t docId    │ uint16_t          │ uint16_t         │ uint8_t     │
│ (4 bytes)         │ bodyTermFreq      │ nameTermFreq     │ fieldMask   │
│                   │ (2 bytes)         │ (2 bytes)        │ (1 byte)    │
├───────────────────┴───────────────────┴──────────────────┴─────────────┤
│ uint8_t padding[7] (Compiler structure padding for 8-byte alignment)   │
├────────────────────────────────────────────────────────────────────────┤
│ PostingNode* next (8 bytes, 64-bit memory address)                     │
└────────────────────────────────────────────────────────────────────────┘
Total Structure Size: 4 + 2 + 2 + 1 + 7 (pad) + 8 = 24 bytes (MSVC x64)
```

#### Detailed Field Specifications:
- `docId` (`uint32_t`, 4 bytes, offset `0x00`): Unique dense document identifier ($1 \dots N$).
- `bodyTermFreq` (`uint16_t`, 2 bytes, offset `0x04`): Number of occurrences of this term within the document's body text (capped at 65,535).
- `nameTermFreq` (`uint16_t`, 2 bytes, offset `0x06`): Number of occurrences of this term within the file's base name (enabling independent field weighting in TF-IDF).
- `fieldMask` (`uint8_t`, 1 byte, offset `0x08`): Bitmask flags:
  - Bit 0 (`0x01`): `FIELD_FILENAME` (Term occurs in base filename).
  - Bit 1 (`0x02`): `FIELD_CONTENT` (Term occurs in body text).
- `padding[7]` (7 bytes, offset `0x09`): Explicit compiler padding ensuring that `next` starts on an 8-byte aligned memory boundary (`offset 0x10`).
- `next` (`PostingNode*`, 8 bytes, offset `0x10`): 64-bit pointer to subsequent posting node in ascending `docId` sequence.

> [!NOTE]
> Under MSVC 2022 x64, `sizeof(PostingNode)` is **exactly 24 bytes**. Any claim of a 12-byte layout is invalid on 64-bit architectures because an 8-byte pointer alone requires 8-byte alignment, forcing the trailing structure size to a multiple of 8.

#### Header Attributes:
- `m_head`: Pointer to first `PostingNode` (`PostingNode*`).
- `m_tail`: Pointer to last `PostingNode` (enables $O(1)$ append during monotonic sequential indexation).
- `m_docFreq`: Total count of documents containing this term ($DF$).

### 4.2 Posting Insertion & Ordering
During corpus indexing, documents are scanned in increasing order of `DocID` ($1, 2, 3, \dots, N$). Consequently, new postings are appended to `m_tail` in deterministic $O(1)$ time:
```
Algorithm AddPosting(list, docId, isFilename):
    if list.m_tail is not nullptr and list.m_tail.docId == docId:
        if isFilename:
            list.m_tail.nameTermFreq = list.m_tail.nameTermFreq + 1
            list.m_tail.fieldMask = list.m_tail.fieldMask | 0x01
        else:
            list.m_tail.bodyTermFreq = list.m_tail.bodyTermFreq + 1
            list.m_tail.fieldMask = list.m_tail.fieldMask | 0x02
        return
    
    newNode = allocate PostingNode()
    newNode.docId = docId
    newNode.nameTermFreq = isFilename ? 1 : 0
    newNode.bodyTermFreq = isFilename ? 0 : 1
    newNode.fieldMask = isFilename ? 0x01 : 0x02
    newNode.next = nullptr

    if list.m_tail is nullptr:
        list.m_head = newNode
        list.m_tail = newNode
    else if docId > list.m_tail.docId:
        list.m_tail.next = newNode
        list.m_tail = newNode
    else:
        // Out-of-order insertion fallback (maintains sorted invariant)
        InsertSorted(list, newNode)
    list.m_docFreq = list.m_docFreq + 1
```

### 4.3 Boolean Set Operations on Sorted Posting Lists

Because all posting lists are strictly ordered by `DocID`, set operations execute in optimal linear time using two-pointer merge techniques, completely avoiding quadratic nested loops.

#### 4.3.1 AND Operation — Set Intersection
Computes the set of documents containing **both** terms:

```
Algorithm Intersect(ListA, ListB):
    ResultList = new CustomPostingList()
    ptrA = ListA.m_head
    ptrB = ListB.m_head

    while ptrA is not nullptr and ptrB is not nullptr:
        if ptrA.docId == ptrB.docId:
            combinedNameFreq = ptrA.nameTermFreq + ptrB.nameTermFreq
            combinedBodyFreq = ptrA.bodyTermFreq + ptrB.bodyTermFreq
            combinedMask = ptrA.fieldMask | ptrB.fieldMask
            ResultList.AppendNode(ptrA.docId, combinedNameFreq, combinedBodyFreq, combinedMask)
            ptrA = ptrA.next
            ptrB = ptrB.next
        else if ptrA.docId < ptrB.docId:
            ptrA = ptrA.next
        else:
            ptrB = ptrB.next

    return ResultList
```
- **Time Complexity:** $O(L_A + L_B)$ where $L_A$ and $L_B$ are the lengths of the respective posting lists.
- **Space Complexity:** $O(\min(L_A, L_B))$ for the newly generated result list.

#### 4.3.2 OR Operation — Set Union
Computes the set of documents containing **either** term:

```
Algorithm Union(ListA, ListB):
    ResultList = new CustomPostingList()
    ptrA = ListA.m_head
    ptrB = ListB.m_head

    while ptrA is not nullptr and ptrB is not nullptr:
        if ptrA.docId == ptrB.docId:
            combinedNameFreq = ptrA.nameTermFreq + ptrB.nameTermFreq
            combinedBodyFreq = ptrA.bodyTermFreq + ptrB.bodyTermFreq
            combinedMask = ptrA.fieldMask | ptrB.fieldMask
            ResultList.AppendNode(ptrA.docId, combinedNameFreq, combinedBodyFreq, combinedMask)
            ptrA = ptrA.next
            ptrB = ptrB.next
        else if ptrA.docId < ptrB.docId:
            ResultList.AppendNode(ptrA.docId, ptrA.nameTermFreq, ptrA.bodyTermFreq, ptrA.fieldMask)
            ptrA = ptrA.next
        else:
            ResultList.AppendNode(ptrB.docId, ptrB.nameTermFreq, ptrB.bodyTermFreq, ptrB.fieldMask)
            ptrB = ptrB.next

    while ptrA is not nullptr:
        ResultList.AppendNode(ptrA.docId, ptrA.nameTermFreq, ptrA.bodyTermFreq, ptrA.fieldMask)
        ptrA = ptrA.next

    while ptrB is not nullptr:
        ResultList.AppendNode(ptrB.docId, ptrB.nameTermFreq, ptrB.bodyTermFreq, ptrB.fieldMask)
        ptrB = ptrB.next

    return ResultList
```
- **Time Complexity:** $O(L_A + L_B)$.
- **Space Complexity:** $O(L_A + L_B)$.

#### 4.3.3 NOT Operation — Set Difference
Computes documents in `ListA` that **do not appear** in `ListB`:

```
Algorithm Difference(ListA, ListB):
    ResultList = new CustomPostingList()
    ptrA = ListA.m_head
    ptrB = ListB.m_head

    while ptrA is not nullptr and ptrB is not nullptr:
        if ptrA.docId == ptrB.docId:
            ptrA = ptrA.next
            ptrB = ptrB.next
        else if ptrA.docId < ptrB.docId:
            ResultList.AppendNode(ptrA.docId, ptrA.nameTermFreq, ptrA.bodyTermFreq, ptrA.fieldMask)
            ptrA = ptrA.next
        else:
            ptrB = ptrB.next

    while ptrA is not nullptr:
        ResultList.AppendNode(ptrA.docId, ptrA.nameTermFreq, ptrA.bodyTermFreq, ptrA.fieldMask)
        ptrA = ptrA.next

    return ResultList
```
- **Time Complexity:** $O(L_A + L_B)$.
- **Space Complexity:** $O(L_A)$.

---

## 5. Custom Trie (`CustomTrie`)

### 5.1 Architectural Purpose & Role in System
The `CustomTrie` (Prefix Tree) manages the corpus vocabulary to deliver keystroke-latency ($< 5\ \text{ms}$) autocomplete suggestions and provide the vocabulary search space for edit-distance spell correction.

### 5.2 Node Structure & Memory Representation
To handle lowercase English alphanumeric terms while remaining compact, each `TrieNode` manages an array of 36 child pointers (`a-z` indices 0–25, `0-9` indices 26–35):

```
┌─────────────────────────────────────────┐
│               TrieNode                  │
├─────────────────────────────────────────┤
│ bool isEndOfWord                        │
│ uint32_t searchFrequency                │
│ TrieNode* children[36]                  │
└─────────────────────────────────────────┘
```
- `isEndOfWord`: Flag indicating if node terminates a valid indexed term.
- `searchFrequency`: Running counter of term frequency (corpus occurrences + search query hits) used for ranked autocomplete ordering.
- `children`: Static array of 36 pointers initialized to `nullptr`.

### 5.3 Core Operations

#### 5.3.1 Term Insertion (`insert(const std::string& word, uint32_t freq)`)
```
Algorithm TrieInsert(root, word, freq):
    current = root
    for each character c in word:
        index = CharToIndex(c)
        if index is invalid:
            continue
        if current.children[index] is nullptr:
            current.children[index] = allocate TrieNode()
        current = current.children[index]
    current.isEndOfWord = true
    current.searchFrequency = current.searchFrequency + freq
```
- **Time Complexity:** $O(L)$ where $L$ is the length of the string.
- **Space Complexity:** $O(L)$ in worst case when allocating new nodes.

#### 5.3.2 Prefix Lookup (`findPrefixNode(const std::string& prefix)`)
Traverses from `root` following character indices of the prefix.
- Returns pointer to the `TrieNode` at the prefix terminus, or `nullptr` if prefix does not exist in vocabulary.
- **Time Complexity:** $O(P)$ where $P$ is prefix length.

#### 5.3.3 Top-5 Suggestion Retrieval via DFS
Once the prefix node is reached, a Depth-First Search collects all valid descendants into candidate suggestions, selecting the 5 highest-frequency completions:

```
Algorithm CollectSuggestions(prefixNode, currentPrefix, candidates):
    if prefixNode.isEndOfWord:
        candidates.push_back(Suggestion(currentPrefix, prefixNode.searchFrequency))
    
    for i from 0 to 35:
        if prefixNode.children[i] is not nullptr:
            nextChar = IndexToChar(i)
            CollectSuggestions(prefixNode.children[i], currentPrefix + nextChar, candidates)

Algorithm GetTop5(root, prefix):
    prefixNode = findPrefixNode(root, prefix)
    if prefixNode is nullptr:
        return empty list
    
    candidates = empty DynamicArray
    CollectSuggestions(prefixNode, prefix, candidates)
    
    // Sort or Heapify candidates to extract top 5 by searchFrequency
    return ExtractTop5ByFrequency(candidates)
```
- **Time Complexity:** $O(P + V_{sub} \log 5)$ where $V_{sub}$ is the number of nodes in the prefix subtree.

### 5.4 Unicode, Extended Characters & Multilingual Search Strategy

#### 5.4.1 Strict Invariant: Zero Information Loss in Inverted Index
A primary architectural guarantee is that **no searchable filename or content information is discarded**, regardless of language or script (e.g., Urdu, Arabic, Cyrillic, CJK, accented Latin).
- The primary search engine operates on **raw UTF-8 byte sequences** in the `CustomHashTable`.
- The 64-bit FNV-1a hash function computes hashes byte-by-byte over arbitrary UTF-8 byte arrays. Consequently, tokens in Urdu (e.g., `دستاویز`, `سبق`), Chinese, or Spanish are fully indexed and retrievable via exact-match and Boolean queries.

#### 5.4.2 36-Way Trie Character Mapping & Boundary
The `CustomTrie` uses a fixed 36-way pointer array (`a-z` indices 0–25, `0-9` indices 26–35) optimized for compact, cache-friendly Latin alphanumeric desktop retrieval:
```
Index 0 .. 25  : 'a' through 'z'
Index 26 .. 35 : '0' through '9'
```

#### 5.4.3 Non-ASCII Character Handling & Fallback Behavior
1. **Punctuation & Delimiters:** Spaces and punctuation are treated as token boundaries by the lexical analyzer; they do not enter Trie nodes.
2. **UTF-8 Normalization:** Strings are normalized to lowercase UTF-8. Non-ASCII multi-byte sequences (bytes with high bit set, `0x80`–`0xFF`) are recognized by the lexer.
3. **Trie Ingestion Filter:** Only tokens composed strictly of the 36 valid ASCII alphanumeric characters are inserted into the `CustomTrie`. Multi-byte Unicode tokens bypass the Trie and are routed directly to the `CustomHashTable` (Inverted Index).
4. **Autocomplete Fallback:** If a user query prefix contains non-ASCII characters (e.g., typing an Urdu or accented word), the autocomplete engine detects the out-of-range character, safely aborts the Trie traversal, and falls back to a linear prefix scan across recent vocabulary or returns an empty suggestion list without crashing.
5. **Spell Correction Limitations:** The Levenshtein distance spell-checker operates over the `CustomTrie` vocabulary. Spell suggestions are therefore formally supported for Latin alphanumeric terms. Non-Latin terms bypass spell correction and are evaluated directly against the inverted index.

---

## 6. Custom Max Heap & Top-K Ranking Strategies (`CustomMaxHeap<T>`)

### 6.1 Architectural Purpose & Role in System
The `CustomMaxHeap` is a complete binary tree satisfying the max-heap invariant: the key of each node is greater than or equal to the keys of its children. In this project, it is used by the `RankingEngine` to extract the Top-$K$ most relevant search results ($K = 50$) out of potentially tens of thousands of candidate matches.

```
       [ Doc 42 : Score 9.8 ] (Index 0)
            /             \
[ Doc 15 : 7.4 ] (1)     [ Doc 88 : 8.1 ] (2)
      /      \                /
[Doc 3:5.2] [Doc 9:6.8]  [Doc 24:4.1]
   (3)         (4)          (5)
```

### 6.2 Array Index Arithmetic
The heap is stored in a `CustomDynamicArray<HeapNode>`. For any element at 0-based index $i$:
$$\text{Parent}(i) = \left\lfloor \frac{i - 1}{2} \right\rfloor$$
$$\text{LeftChild}(i) = 2i + 1$$
$$\text{RightChild}(i) = 2i + 2$$

### 6.3 Heap Node Structure
```
struct HeapNode {
    uint32_t docId;
    float score;
    uint64_t modificationTimestamp; // For deterministic tie-breaking
};
```

#### Tie-Breaking Invariant:
Node $A > B$ if:
$$(A.score > B.score) \lor (A.score == B.score \land A.modificationTimestamp > B.modificationTimestamp) \lor (A.score == B.score \land A.docId < B.docId)$$

### 6.4 Core Operations

#### 6.4.1 Heapify Down (`heapifyDown(size_t index)`)
```
Algorithm HeapifyDown(heap, i):
    size = heap.size()
    largest = i
    left = 2 * i + 1
    right = 2 * i + 2

    if left < size and heap[left] > heap[largest]:
        largest = left
    if right < size and heap[right] > heap[largest]:
        largest = right

    if largest != i:
        Swap(heap[i], heap[largest])
        HeapifyDown(heap, largest)
```
- **Time Complexity:** $O(\log N)$ where $N$ is heap size.

#### 6.4.2 Linear Build Heap (`buildHeap()`)
Constructs a valid heap from an unsorted array of $M$ candidates in linear time by calling `heapifyDown` from the last non-leaf node up to the root:
```
Algorithm BuildHeap(heap):
    for i from (heap.size() / 2 - 1) down to 0:
        HeapifyDown(heap, i)
```
- **Time Complexity:** $O(M)$ where $M$ is candidate count.

#### 6.4.3 Extract Max (`extractMax()`)
```
Algorithm ExtractMax(heap):
    if heap.isEmpty():
        raise UnderflowException
    maxItem = heap[0]
    heap[0] = heap[heap.size() - 1]
    heap.pop_back()
    if not heap.isEmpty():
        HeapifyDown(heap, 0)
    return maxItem
```
- **Time Complexity:** $O(\log N)$.

#### 6.4.4 Comparative Analysis of Top-$K$ Retrieval Strategies

Two distinct heap-based strategies can extract the Top-$K$ ranked documents from a set of $M$ matching candidates.

##### Strategy A: Unbounded Max-Heap ($O(M)$ Build + $K$ Extractions)
1. Load all $M$ candidate `HeapNode` records into a contiguous dynamic array: $O(M)$ time, $O(M)$ auxiliary space.
2. Call `buildHeap()` using Floyd's linear algorithm: $O(M)$ time.
3. Call `extractMax()` exactly $K$ times: $K \times O(\log M)$ time.
- **Total Time Complexity:** $O(M + K \log M)$.
- **Auxiliary Space Complexity:** $O(M)$ heap nodes.
- **Characteristics:** Highly cache-friendly contiguous array traversal. However, memory consumption scales linearly with candidate set size $M$. If a query matches $100,000$ documents, the heap must hold all $100,000$ nodes simultaneously.

##### Strategy B: Bounded Min-Heap of Size $K$ ($O(M \log K)$ Online Filtering)
1. Allocate a heap strictly bounded to capacity $K$: $O(K)$ auxiliary space.
2. For each candidate $(d, score)$ in the candidate stream:
   - If current heap size $< K$: Insert candidate into Min-Heap ($O(\log K)$).
   - If current heap size $== K$: Compare candidate score with heap root (the current $K$-th highest score):
     - If candidate score $\le$ root score: Discard candidate ($O(1)$).
     - If candidate score $>$ root score: Replace root with candidate and call `heapifyDown()` ($O(\log K)$).
3. At completion of candidate iteration, the Min-Heap contains exactly the top $K$ highest-scoring candidates.
4. Extract all $K$ elements in reverse order ($O(K \log K)$) to produce a descending ranked result list.
- **Total Time Complexity:** $O(M \log K)$ worst-case. In practice, as the $K$-th threshold rises, most candidates are rejected in $O(1)$, making average time close to $O(M + K \log K)$.
- **Auxiliary Space Complexity:** Strictly bounded $O(K)$ heap nodes (e.g., exactly 50 nodes $\approx 1.2\ \text{KB}$ regardless of whether $M = 1,000$ or $M = 500,000$).

##### Formal Trade-Off & Recommendation:
| Evaluation Metric | Strategy A (Unbounded Max-Heap) | Strategy B (Bounded Min-Heap) |
| :--- | :--- | :--- |
| **Worst-Case Time** | $O(M + K \log M)$ | $O(M \log K)$ |
| **Auxiliary Memory** | $O(M)$ heap nodes (High for large corpora) | $O(K)$ heap nodes (Strictly bounded) |
| **Peak RAM (100k hits, K=50)** | $\approx 2.4\ \text{MB}$ node buffer | $\approx 1.2\ \text{KB}$ node buffer |
| **Algorithmic Role** | Batch post-processing | Online stream filtering |

> [!NOTE]
> **Architectural Selection:** Both strategies are algorithmically sound. Strategy B (Bounded Min-Heap of size $K$) is selected as the primary ranking pipeline to enforce strict memory bounds on large corpora. Strategy A is retained for batch evaluation when the candidate array is already memory-resident. Claims of arbitrary "15x speedup" are rejected in favor of measured profiling during Phase 10 verification.

---

## 7. Custom Stack (`CustomStack<T>`)

### 7.1 Architectural Purpose & Role in System
The `CustomStack<T>` is a Last-In, First-Out (LIFO) data structure. It is utilized in:
1. **Query Parsing:** Dijkstra's Shunting-Yard algorithm for converting infix Boolean search expressions with parentheses into Reverse Polish Notation (RPN).
2. **Boolean Evaluation:** Holding intermediate posting list pointers while evaluating RPN operator streams.

### 7.2 Implementation Specification
Implemented either via an internal `CustomDynamicArray<T>` or a singly linked list. Dynamic array backing is selected for optimal cache locality and $O(1)$ operations:
```
Class CustomStack<T>:
    m_array: CustomDynamicArray<T>
    
    push(val): m_array.push_back(val)
    pop():     m_array.pop_back()
    top():     return m_array[m_array.size() - 1]
    empty():   return m_array.size() == 0
    size():    return m_array.size()
```
- **Time Complexity:** $O(1)$ amortized for all operations.

---

## 8. Custom Queue (`CustomQueue<T>`)

### 8.1 Architectural Purpose & Justification
The `CustomQueue<T>` is a First-In, First-Out (FIFO) data structure. It plays two essential roles in Release 1:
1. **Breadth-First Search (BFS) Directory Traversal in `FileScanner`:** As directories are discovered, their subdirectories are enqueued. BFS ensures that root-level files and shallow directories are indexed before diving into deep hierarchies, providing immediate initial results.
2. **Token Stream Pipelining:** Feeding parsed postfix query tokens from `QueryParser` to `BooleanSearchEngine`.

### 8.2 Node-Based Queue Design
Implemented via a custom singly linked list with dedicated `m_head` and `m_tail` pointers:
```
Class CustomQueue<T>:
    m_head: QueueNode<T>*
    m_tail: QueueNode<T>*
    m_size: size_t

    enqueue(val):
        node = new QueueNode(val)
        if m_tail == nullptr:
            m_head = m_tail = node
        else:
            m_tail.next = node
            m_tail = node
        m_size++

    dequeue():
        if m_head == nullptr: raise Underflow
        data = m_head.data
        temp = m_head
        m_head = m_head.next
        if m_head == nullptr: m_tail = nullptr
        delete temp
        m_size--
        return data
```
- **Time Complexity:** $O(1)$ deterministic for both `enqueue()` and `dequeue()`.

---

## 9. Inverted Index — Complete Structural Design

### 9.1 Conceptual Mapping Layout
The Inverted Index coordinates the `CustomHashTable`, `CustomPostingList`, and `DocumentCatalog`:

```
Vocabulary Term (std::string)
        │
        ▼ (FNV-1a Hash & Bitmask)
CustomHashTable Bucket Array
        │
        ▼ (Collision Chain Traversal)
HashEntry<std::string, CustomPostingList*>
        │
        ▼ (Pointer Dereference)
CustomPostingList
        │
        ▼ (Sorted by DocID)
PostingNode(DocID 1, TF 4) ──► PostingNode(DocID 7, TF 1) ──► PostingNode(DocID 12, TF 8) ──► nullptr
        │                             │                              │
        ▼ (Direct Array Index)        ▼ (Direct Array Index)         ▼ (Direct Array Index)
DocumentCatalog[1]            DocumentCatalog[7]             DocumentCatalog[12]
(Path: "C:/docs/dsa.txt")     (Path: "C:/src/main.cpp")      (Path: "C:/notes/report.pdf")
```

### 9.2 DocID Lifecycle, Tombstone Consistency & Reference Remapping

#### 9.2.1 Dual Document Identity Model
The system enforces a strict distinction between persistent identity and runtime memory indexation:
1. **Persistent Document Identity:** The canonical absolute filesystem path (`std::string`) stored within the `DocumentCatalog` string pool, uniquely mapped to an active entry via a 64-bit FNV-1a hash.
2. **Dense In-Memory DocID:** A 32-bit unsigned integer ($1, 2, \dots, N$) that acts as a direct $O(1)$ array index into `CustomDynamicArray<FileMetadata>`.

#### 9.2.2 Monotonic ID Assignment & Runtime No-Reuse Policy
- During an active application session, `DocID` values are strictly monotonic: `nextDocId = ++m_highestDocId`.
- **No-Reuse Rule:** `DocID` values are **never reused at runtime**. Even if file `DocID 42` is deleted, `42` is retired for the remainder of the session. This prevents race conditions, eliminates stale pointer traps, and ensures that asynchronous query evaluation never inspects reallocated entries.

#### 9.2.3 Deleted Document Handling (Soft Tombstoning)
When a file is deleted from disk:
- Its corresponding `FileMetadata` record in `DocumentCatalog` sets `flags.is_active = 0`.
- The canonical path entry is removed from the path-to-DocID lookup hash table.
- Individual postings pointing to the deleted `DocID` remain in the posting chains temporarily. At query time, `BooleanSearchEngine` and `RankingEngine` verify `catalog.isActive(docId)` in $O(1)$ before scoring, filtering out tombstoned entries without penalty.

#### 9.2.4 Partial Rebuilding vs. Full Rebuilding
- **Partial Rebuilding (Incremental Sync):**
  When a file is modified:
  1. The existing `DocID` is marked tombstoned (`is_active = 0`).
  2. The updated file is assigned a fresh monotonic `DocID = ++m_highestDocId`.
  3. New metadata and newly extracted content postings are appended to the index.
  4. Cost: $O(T)$ where $T$ is the number of tokens in the modified file. Zero full-index rewriting.
- **Full Rebuilding:**
  The `DocumentCatalog`, `InvertedIndex`, and `CustomTrie` are completely cleared. All memory is freed, and indexing begins afresh starting at `DocID = 1`.

#### 9.2.5 Tombstone Compaction & Reference Remapping Algorithm
During explicit index maintenance or index persistence save, tombstoned entries are purged and `DocID`s are compacted to eliminate sparse gaps. To guarantee that **zero posting references become invalid or dangling**, the compactor executes a two-pass reference remapping:

```
Algorithm CompactAndRemapIndex(catalog, invertedIndex):
    oldTotal = catalog.TotalCount()
    remapTable = allocate uint32_t[oldTotal + 1]
    newDocId = 1

    // Pass 1: Build remap table and compact DocumentCatalog array
    compactCatalog = CustomDynamicArray<FileMetadata>()
    for docId from 1 to oldTotal:
        if catalog.IsActive(docId):
            remapTable[docId] = newDocId
            meta = catalog.GetMetadata(docId)
            meta.docId = newDocId
            compactCatalog.push_back(meta)
            newDocId = newDocId + 1
        else:
            remapTable[docId] = 0 // Marked for deletion

    // Pass 2: Rewrite posting lists in InvertedIndex
    for each bucket in invertedIndex.buckets:
        for each hashEntry in bucket:
            postingList = hashEntry.value
            current = postingList.m_head
            prev = nullptr

            while current is not nullptr:
                mappedId = remapTable[current.docId]
                if mappedId == 0:
                    // Unlink tombstoned posting node
                    toDelete = current
                    if prev is nullptr:
                        postingList.m_head = current.next
                    else:
                        prev.next = current.next
                    if toDelete == postingList.m_tail:
                        postingList.m_tail = prev
                    current = current.next
                    deallocate toDelete
                    postingList.m_docFreq = postingList.m_docFreq - 1
                else:
                    // Remap valid DocID
                    current.docId = mappedId
                    prev = current
                    current = current.next

    catalog.ReplaceData(compactCatalog)
```
- **Post-condition:** Every posting node in the entire inverted index references a valid, active document in the compacted catalog. Zero invalid references exist.

---

## 10. Boolean Operations & Query Parsing Algorithms

### 10.1 Operator Precedence and Grammar
The search grammar supports the following operators:
1. `(` `)` : Parentheses (Highest precedence, evaluated innermost first).
2. `NOT`   : Unary set difference (Precedence 3).
3. `AND`   : Binary set intersection (Precedence 2).
4. `OR`    : Binary set union (Precedence 1, lowest).

Implicit conjunction: Consecutive terms without operators (e.g., `binary search`) are automatically parsed as `binary AND search`.

### 10.2 Shunting-Yard Postfix Conversion Algorithm
```
Algorithm ShuntingYardQueryParser(tokenStream):
    operatorStack = CustomStack<Token>()
    outputQueue = CustomQueue<Token>()

    for each token in tokenStream:
        if token is Term:
            outputQueue.enqueue(token)
        else if token is LeftParen '(':
            operatorStack.push(token)
        else if token is RightParen ')':
            while not operatorStack.empty() and operatorStack.top() is not LeftParen:
                outputQueue.enqueue(operatorStack.pop())
            if operatorStack.empty():
                raise MismatchedParenthesisException
            operatorStack.pop() // Discard '('
        else if token is Operator (AND, OR, NOT):
            while not operatorStack.empty() and 
                  operatorStack.top() is not LeftParen and 
                  Precedence(operatorStack.top()) >= Precedence(token):
                outputQueue.enqueue(operatorStack.pop())
            operatorStack.push(token)

    while not operatorStack.empty():
        if operatorStack.top() is LeftParen:
            raise MismatchedParenthesisException
        outputQueue.enqueue(operatorStack.pop())

    return outputQueue
```

### 10.3 Evaluation of Postfix Query Stream
```
Algorithm EvaluatePostfixQuery(outputQueue, invertedIndex, documentUniverse):
    evalStack = CustomStack<CustomPostingList*>()

    while not outputQueue.empty():
        token = outputQueue.dequeue()
        if token is Term:
            postingList = invertedIndex.Lookup(token.value)
            evalStack.push(postingList)
        else if token is Operator:
            if token == AND:
                listB = evalStack.pop()
                listA = evalStack.pop()
                result = Intersect(listA, listB)
                evalStack.push(result)
            else if token == OR:
                listB = evalStack.pop()
                listA = evalStack.pop()
                result = Union(listA, listB)
                evalStack.push(result)
            else if token == NOT:
                listA = evalStack.pop()
                result = Difference(documentUniverse, listA)
                evalStack.push(result)

    return evalStack.pop() // Final candidate posting list
```

---

## 11. TF-IDF and Heap Integration

### 11.1 Mathematical Formulations with Independent Field Frequencies
For a query $Q = \{t_1, t_2, \dots, t_m\}$ and candidate document $d$, relevance scoring computes separate contributions for filename matches and body content matches:

#### 1. Independent Term Frequencies:
- **Body Term Frequency ($TF_{body}$):** Stored directly in `posting.bodyTermFreq`. Log-normalized to prevent long documents from dominating results:
  $$TF_{body}(t, d) = \begin{cases} 1 + \ln(posting.bodyTermFreq) & \text{if } posting.bodyTermFreq > 0 \\ 0 & \text{otherwise} \end{cases}$$
- **Filename Term Frequency ($TF_{name}$):** Stored directly in `posting.nameTermFreq`. Evaluated with linear weight:
  $$TF_{name}(t, d) = posting.nameTermFreq$$

#### 2. Inverse Document Frequency ($IDF$):
Measures term specificity across the total corpus of $N$ active cataloged documents:
$$IDF(t) = \ln\left(1 + \frac{N}{DF_t}\right)$$
where $DF_t$ is the Document Frequency from `InvertedIndex::getDocumentFrequency(t)`.

#### 3. Field Weight Multipliers:
- $W_{body} = 1.0$: Baseline weight for body content occurrences.
- $W_{name} = 5.0$: Significant elevation factor for filename matches.

#### 4. Recency Modification Bonus ($\text{ModBonus}$):
To gently prioritize fresh files without overriding relevance:
$$\Delta days = \frac{\text{CurrentTime} - \text{lastModifiedTime}}{86400}$$
$$\text{ModBonus}(d) = 1.0 + \frac{0.1}{1.0 + 0.05 \times \Delta days} \quad (1.00 \le \text{ModBonus} \le 1.10)$$

#### 5. Composite Relevance Score Formula:
$$\text{Score}(d, Q) = \left[ \sum_{t \in Q \cap d} \left( \left(TF_{body}(t, d) \times W_{body}\right) + \left(TF_{name}(t, d) \times W_{name}\right) \right) \times IDF(t) \right] \times \text{ModBonus}(d)$$

### 11.2 Heap Scoring Pipeline (Strategy B: Bounded Min-Heap)
```
Algorithm ScoreAndRank(candidatesList, queryTerms, invertedIndex, catalog, K):
    N = catalog.TotalActiveDocuments()
    minHeap = CustomMinHeap<HeapNode>(K) // Capacity bounded strictly to K

    for each candidate in candidatesList:
        docId = candidate.docId
        if not catalog.IsActive(docId):
            continue // Filter tombstoned document in O(1)

        totalScore = 0.0
        for each term in queryTerms:
            posting = invertedIndex.FindPosting(term, docId)
            if posting is not nullptr:
                df = invertedIndex.GetDocFreq(term)
                idf = ln(1.0 + (float(N) / float(df)))
                
                tfBody = (posting.bodyTermFreq > 0) ? (1.0 + ln(posting.bodyTermFreq)) : 0.0
                tfName = float(posting.nameTermFreq)
                
                termScore = (tfBody * 1.0 + tfName * 5.0) * idf
                totalScore += termScore

        metadata = catalog.GetMetadata(docId)
        modBonus = CalculateRecencyBonus(metadata.lastModifiedTime)
        finalScore = totalScore * modBonus

        node = HeapNode(docId, finalScore, metadata.lastModifiedTime)
        
        if minHeap.Size() < K:
            minHeap.Insert(node)
        else if minHeap.Root().score < finalScore:
            minHeap.ReplaceRoot(node) // Bounded maintenance in O(log K)

    // Extract K results in descending order
    topResults = DynamicArray<SearchResultDTO>()
    while not minHeap.IsEmpty():
        topResults.push_front(FormatResult(minHeap.ExtractRoot(), catalog))

    return topResults
```

---

## 12. Complete Complexity Analysis Table

| Data Structure | Key Operation | Average / Typical Complexity | Worst-Case Complexity | Underlying Mathematical Assumptions |
| :--- | :--- | :---: | :---: | :--- |
| **Custom Singly Linked List** | `push_front` | $O(1)$ | $O(1)$ | Direct pointer assignment |
| | `push_back` | $O(1)$ | $O(1)$ | Maintained `tail` pointer |
| | `remove_if` | $O(N)$ | $O(N)$ | Linear traversal to locate predecessor |
| **Custom Dynamic Array** | Access `operator[]` | $O(1)$ | $O(1)$ | Contiguous pointer offset arithmetic |
| | `push_back` | $O(1)$ amortized | $O(N)$ | Geometric expansion ($1.5\times$ growth factor) |
| | Shrink / Realloc | $O(N)$ | $O(N)$ | Buffer reallocation & element copying |
| **Custom Hash Table** | Search (`find`) | $O(1)$ | $O(N)$ | Uniform FNV-1a distribution; $\alpha \le 0.75$ |
| | Insertion (`insert`) | $O(1)$ amortized | $O(N)$ | $O(1)$ chain insertion; periodic $O(N)$ rehash |
| | Deletion (`remove`) | $O(1)$ | $O(N)$ | Direct unlinking from short bucket chain |
| **Custom Posting List** | Append (Sorted Ingestion)| $O(1)$ | $O(1)$ | DocIDs arrive in monotonic order during scan |
| | Intersect (`AND`) | $O(L_1 + L_2)$ | $O(L_1 + L_2)$ | Both lists strictly sorted by DocID |
| | Union (`OR`) | $O(L_1 + L_2)$ | $O(L_1 + L_2)$ | Two-pointer simultaneous linear sweep |
| | Difference (`NOT`) | $O(L_1 + L_2)$ | $O(L_1 + L_2)$ | Linear sorted traversal |
| **Custom Trie** | Prefix Search | $O(P)$ | $O(P)$ | $P = \text{length of prefix}$; independent of $N$ |
| | Top-5 Retrieval | $O(P + V_{sub} \log 5)$ | $O(P + V_{sub} \log 5)$ | $V_{sub} = \text{nodes in prefix subtree}$ |
| **Custom Max Heap** | Insert | $O(\log N)$ | $O(\log N)$ | Complete binary tree height $\lfloor \log_2 N \rfloor$ |
| | Extract Max | $O(\log N)$ | $O(\log N)$ | Single path `heapifyDown` along tree height |
| | Build Heap | $O(M)$ | $O(M)$ | $\sum_{h=0}^{\lfloor\log M\rfloor} \frac{M}{2^{h+1}} O(h) = O(M)$ |
| | Top-$K$ Selection | $O(M + K \log M)$ | $O(M + K \log M)$ | Linear build followed by $K$ extractions |
| **Custom Stack** | Push / Pop | $O(1)$ | $O(1)$ | Dynamic array back operations |
| **Custom Queue** | Enqueue / Dequeue | $O(1)$ | $O(1)$ | Node pointer manipulation at head/tail |

---

## 13. Memory Management & Safety Strategy

### 13.1 Pointer Ownership Architecture
To ensure zero memory leaks and eliminate dangling pointers, the system enforces unambiguous, single-owner hierarchies:

```
IndexingCoordinator (Owner)
  └── DocumentCatalog (Owner)
        └── CustomDynamicArray<FileMetadata> (Contiguous Buffer Owner)
  └── InvertedIndex (Owner)
        └── CustomHashTable (Bucket Array Owner)
              └── HashEntry Nodes (Owner of Key and PostingList)
                    └── CustomPostingList (Owner)
                          └── PostingNode Chain (Deep Owner)
  └── TrieAutocomplete (Owner)
        └── CustomTrie (Root Owner)
              └── TrieNode Recursive Subtree (Deep Owner)
```

### 13.2 RAII & Deep Copy Semantics
1. **Rule of Five:** Every custom data structure explicitly defines:
   - Destructor (deep deallocation of all internal nodes).
   - Copy Constructor (deep copy of elements/nodes).
   - Copy Assignment Operator (copy-and-swap idiom for strong exception safety).
   - Move Constructor (stealing internal pointers and resetting source to `nullptr`).
   - Move Assignment Operator (resource swap and safe deallocation).
2. **Intermediate Posting Lists in Boolean Evaluation:**
   When evaluating Boolean operations (`Intersect`, `Union`, `Difference`), temporary `CustomPostingList` instances are returned by value (moving resources) or wrapped in an RAII stack manager that cleans up all intermediate temporary lists immediately once the query finishes.

### 13.3 Memory Sanitization & Verification Plan
- **MSVC CRT Leak Detection:** Embedded `#define _CRTDBG_MAP_ALLOC` with `_CrtDumpMemoryLeaks()` at application exit.
- **Valgrind / AddressSanitizer (ASan):** Tested via `-fsanitize=address` to verify zero out-of-bounds array reads and zero use-after-free events.

---

## 14. DSA Testing Strategy

### 14.1 Unit Testing Matrix by Structure

| Structure | Normal Verification Cases | Boundary & Edge Cases | Large Dataset Stress Tests | Memory Safety Verification |
| :--- | :--- | :--- | :--- | :--- |
| **Custom Singly Linked List** | Sequential appends, standard deletion by value, forwards iteration | Empty list delete, single-element list remove head/tail, duplicate values | 500,000 node chain traversal and destruction | Verification of non-recursive iterative destruction (zero stack overflow) |
| **Custom Dynamic Array** | Element insertion, indexing, resizing across initial capacity | Access index 0 on 1-element array, out-of-bounds exception check, capacity shrinkage | 1,000,000 elements pushed; verify 1.5x reallocation overhead | Zero double-free on copy assignment; move constructor leaves donor empty |
| **Custom Hash Table** | 10,000 unique key insertions, retrieval, key updates | Colliding keys mapping to same bucket, empty string key, lookup of non-existent key | 200,000 keys inserted to trigger multiple dynamic rehashes at $\alpha > 0.75$ | Spliced node rehash preserves pointer stability; 100% memory freed on clear |
| **Custom Posting List** | Ordered DocID appends, duplicate DocID frequency increment | Intersection of disjoint lists, intersection with empty list, identical list intersection | Merge-intersect two lists of 100,000 postings each | Temporary result list deallocation cleanly verified |
| **Custom Trie** | Insert 20 words, prefix lookup, suggestion retrieval | Prefix matching entire word, prefix matching nothing, 1-character prefix | Corpus of 50,000 distinct words loaded; verify Top-5 suggestion latency $< 5\ \text{ms}$ | Recursive destruction frees every node; invalid characters safely skipped |
| **Custom Max Heap** | Insert scores, verify Max at index 0, sequential `extractMax` descending | Single element heap, duplicate scores (verify timestamp tie-breaking) | 100,000 candidate nodes; verify `buildHeap` executes in $< 15\ \text{ms}$ | Boundary check on `extractMax` on empty heap (graceful exception) |
| **Custom Stack** | Standard Push/Pop sequence, LIFO order verification | Pop on empty stack, single-element stack pop | 50,000 nested expression tokens pushed and popped | Exception safety on stack underflow |
| **Custom Queue** | Sequential Enqueue/Dequeue, FIFO ordering | Dequeue on empty queue, interleaved enqueue/dequeue | 100,000 filesystem directory paths processed in BFS order | Correct head/tail reset when last element dequeued |

---

## 15. Open Architecture Decisions Requiring Approval

The following design decisions are recorded for academic/project review:

1. **[DECISION-DSA-01] Trie Node Branching Factor (36-way vs. Dynamic Linked Map):**  
   *Current Design:* Static 36-way pointer array (`a-z`, `0-9`) per node for $O(1)$ transitions.  
   *Trade-off:* 36 pointers $\times$ 8 bytes = 288 bytes per node. For 100,000 nodes $\approx 28.8\ \text{MB}$ of RAM.  
   *Alternative:* Compressed Radix Trie or linked child list.  
   *Recommendation:* Retain 36-way array for Release 1 to maximize algorithmic clarity and deterministic $O(1)$ transition speed, perfectly fitting within our $< 350\ \text{MB}$ RAM budget.

2. **[DECISION-DSA-02] Posting List Storage (Singly Linked List vs. Unrolled Linked List):**  
   *Current Design:* Standard singly linked list of `PostingNode`s.  
   *Alternative:* Unrolled linked list (chunks of 16 DocIDs per node for cache locality).  
   *Recommendation:* Standard singly linked list satisfies the core DSA coursework requirements cleanly and makes merge-intersection algorithms straightforward to evaluate.
