#include "search/SpellChecker.hpp"
#include <algorithm>
#include <cctype>

namespace sse::search {

SpellChecker::SpellChecker(const dsa::CustomTrie* trie) noexcept
    : m_trie(trie) {}

std::string SpellChecker::normalizeWord(const std::string& word) {
    std::string result;
    result.reserve(word.size());
    for (char c : word) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            result.push_back(c);
        } else if (c >= 'A' && c <= 'Z') {
            result.push_back(static_cast<char>(c + 32));
        }
    }
    return result;
}

std::size_t SpellChecker::computeLevenshteinDistance(const std::string& s1, const std::string& s2) {
    const std::size_t m = s1.size();
    const std::size_t n = s2.size();

    if (m == 0) return n;
    if (n == 0) return m;

    dsa::CustomDynamicArray<std::size_t> prev(n + 1);
    dsa::CustomDynamicArray<std::size_t> curr(n + 1);

    for (std::size_t j = 0; j <= n; ++j) {
        prev[j] = j;
    }

    for (std::size_t i = 1; i <= m; ++i) {
        curr[0] = i;
        char c1 = static_cast<char>(std::tolower(static_cast<unsigned char>(s1[i - 1])));

        for (std::size_t j = 1; j <= n; ++j) {
            char c2 = static_cast<char>(std::tolower(static_cast<unsigned char>(s2[j - 1])));
            std::size_t cost = (c1 == c2) ? 0 : 1;

            std::size_t delCost = prev[j] + 1;
            std::size_t insCost = curr[j - 1] + 1;
            std::size_t subCost = prev[j - 1] + cost;

            curr[j] = std::min({delCost, insCost, subCost});
        }

        for (std::size_t j = 0; j <= n; ++j) {
            prev[j] = curr[j];
        }
    }

    return prev[n];
}

void SpellChecker::searchRecursive(const dsa::TrieNode* node,
                                   char ch,
                                   const std::string& target,
                                   const dsa::CustomDynamicArray<std::size_t>& prevRow,
                                   std::string& currentPath,
                                   std::size_t maxDistance,
                                   dsa::CustomDynamicArray<SpellSuggestion>& candidates) const {
    if (!node) return;

    currentPath.push_back(ch);

    const std::size_t targetLen = target.size();
    dsa::CustomDynamicArray<std::size_t> currentRow(targetLen + 1);
    currentRow[0] = prevRow[0] + 1;
    std::size_t minRowVal = currentRow[0];

    for (std::size_t j = 1; j <= targetLen; ++j) {
        std::size_t insertCost = currentRow[j - 1] + 1;
        std::size_t deleteCost = prevRow[j] + 1;
        std::size_t replaceCost = prevRow[j - 1] + ((target[j - 1] == ch) ? 0 : 1);

        currentRow[j] = std::min({insertCost, deleteCost, replaceCost});
        if (currentRow[j] < minRowVal) {
            minRowVal = currentRow[j];
        }
    }

    if (node->isEndOfWord && currentRow[targetLen] <= maxDistance) {
        candidates.push_back(SpellSuggestion(currentPath,
                                             static_cast<uint32_t>(currentRow[targetLen]),
                                             node->searchFrequency));
    }

    if (minRowVal <= maxDistance) {
        for (std::size_t i = 0; i < dsa::CustomTrie::ALPHABET_SIZE; ++i) {
            if (node->children[i]) {
                char nextChar = dsa::CustomTrie::indexToChar(i);
                searchRecursive(node->children[i], nextChar, target, currentRow,
                                currentPath, maxDistance, candidates);
            }
        }
    }

    currentPath.pop_back();
}

dsa::CustomDynamicArray<SpellSuggestion> SpellChecker::suggestDetailed(const std::string& word,
                                                                     std::size_t maxDistance,
                                                                     std::size_t maxSuggestions) const {
    dsa::CustomDynamicArray<SpellSuggestion> result;
    if (!m_trie || !m_trie->root() || m_trie->empty() || maxSuggestions == 0) {
        return result;
    }

    std::string target = normalizeWord(word);
    if (target.empty()) {
        return result;
    }

    const std::size_t targetLen = target.size();
    dsa::CustomDynamicArray<std::size_t> initialRow(targetLen + 1);
    for (std::size_t i = 0; i <= targetLen; ++i) {
        initialRow[i] = i;
    }

    dsa::CustomDynamicArray<SpellSuggestion> candidates;
    candidates.reserve(32);
    std::string currentPath;
    currentPath.reserve(32);

    const dsa::TrieNode* rootNode = m_trie->root();
    for (std::size_t i = 0; i < dsa::CustomTrie::ALPHABET_SIZE; ++i) {
        if (rootNode->children[i]) {
            char ch = dsa::CustomTrie::indexToChar(i);
            searchRecursive(rootNode->children[i], ch, target, initialRow,
                            currentPath, maxDistance, candidates);
        }
    }

    if (candidates.empty()) {
        return result;
    }

    std::sort(candidates.begin(), candidates.end());

    std::size_t count = std::min(candidates.size(), maxSuggestions);
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        result.push_back(std::move(candidates[i]));
    }

    return result;
}

dsa::CustomDynamicArray<std::string> SpellChecker::suggest(const std::string& word,
                                                         std::size_t maxDistance,
                                                         std::size_t maxSuggestions) const {
    dsa::CustomDynamicArray<std::string> result;
    auto detailed = suggestDetailed(word, maxDistance, maxSuggestions);
    result.reserve(detailed.size());
    for (std::size_t i = 0; i < detailed.size(); ++i) {
        result.push_back(std::move(detailed[i].term));
    }
    return result;
}

} // namespace sse::search
