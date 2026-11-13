#pragma once

#include <string>
#include <cstddef>
#include <cstdint>
#include "dsa/CustomDynamicArray.hpp"
#include "dsa/CustomTrie.hpp"

namespace sse::search {

/**
 * @brief Spell-correction candidate containing term, Levenshtein edit distance, and corpus frequency.
 */
struct SpellSuggestion {
    std::string term;
    uint32_t    distance{0};
    uint32_t    frequency{0};

    SpellSuggestion() = default;
    SpellSuggestion(std::string t, uint32_t dist, uint32_t freq)
        : term(std::move(t)), distance(dist), frequency(freq) {}

    /**
     * @brief Deterministic comparator:
     * 1. Lower edit distance preferred.
     * 2. Higher corpus frequency preferred.
     * 3. Lexicographical ascending tie-breaker.
     */
    bool operator<(const SpellSuggestion& other) const noexcept {
        if (distance != other.distance) {
            return distance < other.distance;
        }
        if (frequency != other.frequency) {
            return frequency > other.frequency;
        }
        return term < other.term;
    }

    bool operator>(const SpellSuggestion& other) const noexcept {
        return other < *this;
    }

    bool operator==(const SpellSuggestion& other) const noexcept {
        return distance == other.distance && frequency == other.frequency && term == other.term;
    }
};

/**
 * @brief High-performance spell checker exploring Trie vocabulary using Levenshtein distance dynamic programming.
 * 
 * Satisfies AC-10 with strict Zero-STL policy across core algorithmic structures.
 * Bounded by Levenshtein distance <= 2 with aggressive subtree pruning.
 */
class SpellChecker {
public:
    explicit SpellChecker(const dsa::CustomTrie* trie) noexcept;
    ~SpellChecker() = default;

    // Rule of Five
    SpellChecker(const SpellChecker&) = default;
    SpellChecker& operator=(const SpellChecker&) = default;
    SpellChecker(SpellChecker&&) noexcept = default;
    SpellChecker& operator=(SpellChecker&&) noexcept = default;

    /**
     * @brief Pure algorithmic Levenshtein distance calculation between two arbitrary strings.
     * Time: O(|s1| * |s2|), Space: O(|s2|). Zero STL.
     */
    static std::size_t computeLevenshteinDistance(const std::string& s1, const std::string& s2);

    /**
     * @brief Normalizes input word to lowercase ASCII alphanumeric.
     */
    static std::string normalizeWord(const std::string& word);

    /**
     * @brief Finds ranked term suggestions within edit distance <= maxDistance.
     * @param word Raw user query term.
     * @param maxDistance Maximum allowed Levenshtein distance (default 2).
     * @param maxSuggestions Maximum candidates to return (default 5).
     * @return CustomDynamicArray<std::string> Ordered suggestion terms.
     */
    dsa::CustomDynamicArray<std::string> suggest(const std::string& word,
                                                 std::size_t maxDistance = 2,
                                                 std::size_t maxSuggestions = 5) const;

    /**
     * @brief Detailed spell suggestions including distance and frequency.
     */
    dsa::CustomDynamicArray<SpellSuggestion> suggestDetailed(const std::string& word,
                                                             std::size_t maxDistance = 2,
                                                             std::size_t maxSuggestions = 5) const;

private:
    const dsa::CustomTrie* m_trie{nullptr};

    void searchRecursive(const dsa::TrieNode* node,
                         char ch,
                         const std::string& target,
                         const dsa::CustomDynamicArray<std::size_t>& prevRow,
                         std::string& currentPath,
                         std::size_t maxDistance,
                         dsa::CustomDynamicArray<SpellSuggestion>& candidates) const;
};

} // namespace sse::search
