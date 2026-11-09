#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include "dsa/CustomDynamicArray.hpp"

namespace sse::dsa {

/**
 * @brief Auto-complete suggestion candidate containing term and ranking frequency.
 */
struct Suggestion {
    std::string term;
    uint32_t frequency{0};

    Suggestion() = default;
    Suggestion(std::string t, uint32_t f) : term(std::move(t)), frequency(f) {}

    bool operator>(const Suggestion& other) const noexcept {
        if (frequency != other.frequency) {
            return frequency > other.frequency;
        }
        return term < other.term; // Tie-breaker: lexicographical order
    }
};

/**
 * @brief 36-way Trie node for ASCII lowercase letters [a-z] and digits [0-9].
 */
struct TrieNode {
    static constexpr std::size_t ALPHABET_SIZE = 36;

    bool isEndOfWord{false};
    uint32_t searchFrequency{0};
    TrieNode* children[ALPHABET_SIZE]{nullptr};

    TrieNode() noexcept = default;
    ~TrieNode() = default;
};

/**
 * @brief 36-way Custom Trie for prefix search, keystroke autocomplete (<5ms),
 * and spell-correction vocabulary space.
 * 
 * Complies with strict invariant:
 * - Maps lowercase ASCII [a-z] (indices 0..25) and [0-9] (indices 26..35).
 * - Multi-byte non-ASCII characters safely bypass Trie and are handled by CustomHashTable.
 */
class CustomTrie {
public:
    static constexpr std::size_t ALPHABET_SIZE = 36;

    CustomTrie() noexcept;
    ~CustomTrie() noexcept;

    // Rule of Five
    CustomTrie(const CustomTrie& other);
    CustomTrie(CustomTrie&& other) noexcept;
    CustomTrie& operator=(const CustomTrie& other);
    CustomTrie& operator=(CustomTrie&& other) noexcept;

    /**
     * @brief Inserts a word into the Trie with an associated occurrence/search frequency.
     * Normalizes uppercase ASCII to lowercase.
     * If word contains invalid characters (non-alphanumeric), returns false.
     */
    bool insert(const std::string& word, uint32_t freq = 1);

    /**
     * @brief Removes a word from the Trie if present.
     * Decrements wordCount and clears terminal flag.
     * @return true if word was terminal and removed; false if absent or invalid.
     */
    bool remove(const std::string& word);

    /**
     * @brief Checks if exact word is present in the Trie.
     */
    bool contains(const std::string& word) const noexcept;

    /**
     * @brief Retrieves the search frequency for an exact word match, or 0 if absent.
     */
    uint32_t getFrequency(const std::string& word) const noexcept;

    /**
     * @brief Checks if any word starts with the given prefix.
     */
    bool hasPrefix(const std::string& prefix) const noexcept;

    /**
     * @brief Returns pointer to the TrieNode corresponding to the prefix, or nullptr.
     */
    const TrieNode* findPrefixNode(const std::string& prefix) const noexcept;

    /**
     * @brief Collects top-K (default 5) suggestions ordered by searchFrequency descending.
     */
    CustomDynamicArray<Suggestion> getTopKSuggestions(const std::string& prefix, std::size_t k = 5) const;

    /**
     * @brief Total number of distinct terminal words in Trie.
     */
    std::size_t wordCount() const noexcept { return m_wordCount; }

    /**
     * @brief Total allocated Trie nodes.
     */
    std::size_t nodeCount() const noexcept { return m_nodeCount; }

    /**
     * @brief Returns true if Trie contains no words.
     */
    bool empty() const noexcept { return m_wordCount == 0; }

    /**
     * @brief Deallocates all Trie nodes and resets counts.
     */
    void clear() noexcept;

    /**
     * @brief Swaps contents with another Trie.
     */
    void swap(CustomTrie& other) noexcept;

    // =========================================================================
    // Static Utility Functions
    // =========================================================================

    /**
     * @brief Maps [a-z, A-Z, 0-9] to [0..35]. Returns -1 for invalid chars.
     */
    static int charToIndex(char c) noexcept;

    /**
     * @brief Maps [0..35] back to character [a-z, 0-9].
     */
    static char indexToChar(std::size_t index) noexcept;

    /**
     * @brief Checks if a string contains exclusively ASCII alphanumeric characters.
     */
    static bool isValidAsciiWord(const std::string& word) noexcept;

private:
    TrieNode*   m_root{nullptr};
    std::size_t m_wordCount{0};
    std::size_t m_nodeCount{0};

    // Helper functions
    void destroySubtree(TrieNode* node) noexcept;
    TrieNode* cloneSubtree(const TrieNode* node, std::size_t& outNodeCount);
    void collectSuggestionsDfs(const TrieNode* node, std::string& currentTerm,
                               CustomDynamicArray<Suggestion>& candidates) const;
};

} // namespace sse::dsa

namespace sse {
    using Trie = sse::dsa::CustomTrie;
}
