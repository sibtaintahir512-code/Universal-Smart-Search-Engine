#pragma once

#include <string>
#include <cstdint>
#include <cstddef>
#include <shared_mutex>
#include <functional>
#include "core/CommonTypes.hpp"
#include "dsa/CustomHashTable.hpp"
#include "dsa/CustomPostingList.hpp"
#include "dsa/CustomTrie.hpp"
#include "dsa/CustomDynamicArray.hpp"

namespace sse::indexing {

/**
 * @brief In-memory Inverted Index subsystem.
 * 
 * Maps normalized vocabulary terms to sorted DocID posting lists via CustomHashTable.
 * Coordinates vocabulary synchronization with CustomTrie for ASCII [a-z0-9] terms.
 * Fully owns all dynamically allocated CustomPostingList instances.
 */
class InvertedIndex {
public:
    InvertedIndex();
    ~InvertedIndex() noexcept;

    // Rule of Five (Non-copyable due to mutex synchronization, movable)
    InvertedIndex(const InvertedIndex&) = delete;
    InvertedIndex& operator=(const InvertedIndex&) = delete;
    InvertedIndex(InvertedIndex&& other) noexcept;
    InvertedIndex& operator=(InvertedIndex&& other) noexcept;

    // =========================================================================
    // Modification APIs
    // =========================================================================

    /**
     * @brief Adds or merges a term occurrence for a specific document.
     * @param term Normalized term string.
     * @param docId Dense valid document identifier.
     * @param nameFreq Occurrences in filename.
     * @param bodyFreq Occurrences in body content.
     * @param fieldMask Bitmask flags (FIELD_FILENAME | FIELD_CONTENT).
     */
    void addTerm(const std::string& term,
                 DocID docId,
                 uint16_t nameFreq,
                 uint16_t bodyFreq,
                 uint8_t fieldMask);

    /**
     * @brief Removes a document's postings across the entire inverted index.
     * Cleans up empty posting lists, updates total postings count, and prunes Trie entries.
     * @param docId Target document identifier to remove.
     * @return true if document was present and removed; false otherwise.
     */
    bool removeDocument(DocID docId);

    /**
     * @brief Deletes a specific term from the index and deallocates its posting list.
     */
    bool removeTerm(const std::string& term);

    /**
     * @brief Clears all terms, posting lists, and Trie vocabulary.
     */
    void clear() noexcept;

    // =========================================================================
    // Retrieval APIs
    // =========================================================================

    /**
     * @brief Retrieves borrowed pointer to a term's posting list in O(1) time.
     * Returns nullptr if term is not indexed.
     */
    const dsa::CustomPostingList* getPostingList(const std::string& term) const noexcept;
    dsa::CustomPostingList* getMutablePostingList(const std::string& term) noexcept;

    /**
     * @brief Retrieves document frequency (DF) for a term in O(1) time.
     */
    uint32_t getDocumentFrequency(const std::string& term) const noexcept;

    /**
     * @brief Checks if a term exists in the vocabulary.
     */
    bool containsTerm(const std::string& term) const noexcept;

    /**
     * @brief Total number of unique terms in the primary hash table vocabulary.
     */
    std::size_t vocabularySize() const noexcept;

    /**
     * @brief Total number of terms registered in the Latin alphanumeric Trie.
     */
    std::size_t trieVocabularySize() const noexcept;

    /**
     * @brief Total count of posting nodes across all indexed terms.
     */
    uint64_t totalPostingsCount() const noexcept;

    /**
     * @brief Access to vocabulary Trie.
     */
    const dsa::CustomTrie& trie() const noexcept { return m_trie; }

    /**
     * @brief Diagnostic iteration across all indexed terms and their posting lists.
     */
    void forEachTerm(const std::function<void(const std::string& term, const dsa::CustomPostingList& list)>& callback) const;

private:
    mutable std::shared_mutex                                 m_mutex;
    dsa::CustomHashTable<std::string, dsa::CustomPostingList*> m_table;
    dsa::CustomTrie                                           m_trie;
    uint64_t                                                  m_totalPostings{0};

    void deallocateAllLists() noexcept;
};

} // namespace sse::indexing
