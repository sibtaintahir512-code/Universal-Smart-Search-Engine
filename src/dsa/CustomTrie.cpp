#include "dsa/CustomTrie.hpp"
#include <algorithm>

namespace sse::dsa {

CustomTrie::CustomTrie() noexcept = default;

CustomTrie::~CustomTrie() noexcept {
    clear();
}

CustomTrie::CustomTrie(const CustomTrie& other) {
    if (other.m_root) {
        std::size_t count = 0;
        m_root = cloneSubtree(other.m_root, count);
        m_nodeCount = count;
        m_wordCount = other.m_wordCount;
    }
}

CustomTrie::CustomTrie(CustomTrie&& other) noexcept
    : m_root(other.m_root),
      m_wordCount(other.m_wordCount),
      m_nodeCount(other.m_nodeCount) {
    other.m_root = nullptr;
    other.m_wordCount = 0;
    other.m_nodeCount = 0;
}

CustomTrie& CustomTrie::operator=(const CustomTrie& other) {
    if (this != &other) {
        CustomTrie temp(other);
        swap(temp);
    }
    return *this;
}

CustomTrie& CustomTrie::operator=(CustomTrie&& other) noexcept {
    if (this != &other) {
        clear();
        m_root = other.m_root;
        m_wordCount = other.m_wordCount;
        m_nodeCount = other.m_nodeCount;

        other.m_root = nullptr;
        other.m_wordCount = 0;
        other.m_nodeCount = 0;
    }
    return *this;
}

void CustomTrie::swap(CustomTrie& other) noexcept {
    std::swap(m_root, other.m_root);
    std::swap(m_wordCount, other.m_wordCount);
    std::swap(m_nodeCount, other.m_nodeCount);
}

int CustomTrie::charToIndex(char c) noexcept {
    if (c >= 'a' && c <= 'z') {
        return c - 'a';
    }
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= '0' && c <= '9') {
        return 26 + (c - '0');
    }
    return -1;
}

char CustomTrie::indexToChar(std::size_t index) noexcept {
    if (index < 26) {
        return static_cast<char>('a' + index);
    }
    if (index < ALPHABET_SIZE) {
        return static_cast<char>('0' + (index - 26));
    }
    return '?';
}

bool CustomTrie::isValidAsciiWord(const std::string& word) noexcept {
    if (word.empty()) {
        return false;
    }
    for (char c : word) {
        if (charToIndex(c) == -1) {
            return false;
        }
    }
    return true;
}

bool CustomTrie::insert(const std::string& word, uint32_t freq) {
    if (!isValidAsciiWord(word)) {
        return false;
    }

    if (!m_root) {
        m_root = new TrieNode();
        m_nodeCount = 1;
    }

    TrieNode* current = m_root;
    for (char c : word) {
        int idx = charToIndex(c);
        if (!current->children[idx]) {
            current->children[idx] = new TrieNode();
            ++m_nodeCount;
        }
        current = current->children[idx];
    }

    if (!current->isEndOfWord) {
        current->isEndOfWord = true;
        ++m_wordCount;
    }
    current->searchFrequency += freq;
    return true;
}

bool CustomTrie::remove(const std::string& word) {
    if (word.empty() || !m_root || !isValidAsciiWord(word)) {
        return false;
    }

    TrieNode* current = m_root;
    for (char c : word) {
        int idx = charToIndex(c);
        if (idx == -1 || !current->children[idx]) {
            return false;
        }
        current = current->children[idx];
    }

    if (current && current->isEndOfWord) {
        current->isEndOfWord = false;
        current->searchFrequency = 0;
        if (m_wordCount > 0) {
            --m_wordCount;
        }
        return true;
    }

    return false;
}

bool CustomTrie::contains(const std::string& word) const noexcept {
    if (word.empty() || !m_root) {
        return false;
    }

    const TrieNode* current = m_root;
    for (char c : word) {
        int idx = charToIndex(c);
        if (idx == -1 || !current->children[idx]) {
            return false;
        }
        current = current->children[idx];
    }

    return current && current->isEndOfWord;
}

uint32_t CustomTrie::getFrequency(const std::string& word) const noexcept {
    if (word.empty() || !m_root) {
        return 0;
    }

    const TrieNode* current = m_root;
    for (char c : word) {
        int idx = charToIndex(c);
        if (idx == -1 || !current->children[idx]) {
            return 0;
        }
        current = current->children[idx];
    }

    return (current && current->isEndOfWord) ? current->searchFrequency : 0;
}

bool CustomTrie::hasPrefix(const std::string& prefix) const noexcept {
    return findPrefixNode(prefix) != nullptr;
}

const TrieNode* CustomTrie::findPrefixNode(const std::string& prefix) const noexcept {
    if (!m_root) {
        return nullptr;
    }

    const TrieNode* current = m_root;
    for (char c : prefix) {
        int idx = charToIndex(c);
        if (idx == -1 || !current->children[idx]) {
            return nullptr;
        }
        current = current->children[idx];
    }

    return current;
}

void CustomTrie::collectSuggestionsDfs(const TrieNode* node, std::string& currentTerm,
                                      CustomDynamicArray<Suggestion>& candidates) const {
    if (!node) {
        return;
    }

    if (node->isEndOfWord) {
        candidates.push_back(Suggestion(currentTerm, node->searchFrequency));
    }

    for (std::size_t i = 0; i < ALPHABET_SIZE; ++i) {
        if (node->children[i]) {
            currentTerm.push_back(indexToChar(i));
            collectSuggestionsDfs(node->children[i], currentTerm, candidates);
            currentTerm.pop_back();
        }
    }
}

CustomDynamicArray<Suggestion> CustomTrie::getTopKSuggestions(const std::string& prefix, std::size_t k) const {
    CustomDynamicArray<Suggestion> result;
    if (k == 0) {
        return result;
    }

    // Non-ASCII fallback per Section 5.4.3: if invalid chars, safely return empty
    for (char c : prefix) {
        if (charToIndex(c) == -1) {
            return result;
        }
    }

    const TrieNode* startNode = findPrefixNode(prefix);
    if (!startNode) {
        return result;
    }

    // Lowercase normalized prefix string
    std::string normalizedPrefix;
    normalizedPrefix.reserve(prefix.size());
    for (char c : prefix) {
        int idx = charToIndex(c);
        normalizedPrefix.push_back(indexToChar(static_cast<std::size_t>(idx)));
    }

    CustomDynamicArray<Suggestion> candidates;
    collectSuggestionsDfs(startNode, normalizedPrefix, candidates);

    if (candidates.empty()) {
        return result;
    }

    // Simple in-place sort of candidates by frequency descending, then term ascending
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        for (std::size_t j = i + 1; j < candidates.size(); ++j) {
            if (candidates[j] > candidates[i]) {
                std::swap(candidates[i], candidates[j]);
            }
        }
    }

    std::size_t take = std::min(k, candidates.size());
    result.reserve(take);
    for (std::size_t i = 0; i < take; ++i) {
        result.push_back(candidates[i]);
    }

    return result;
}

void CustomTrie::clear() noexcept {
    if (m_root) {
        destroySubtree(m_root);
        m_root = nullptr;
    }
    m_wordCount = 0;
    m_nodeCount = 0;
}

void CustomTrie::destroySubtree(TrieNode* rootNode) noexcept {
    if (!rootNode) return;

    // Iterative stack cleanup using dynamic array
    CustomDynamicArray<TrieNode*> stack;
    stack.push_back(rootNode);

    while (!stack.empty()) {
        TrieNode* curr = stack.back();
        stack.pop_back();

        for (std::size_t i = 0; i < ALPHABET_SIZE; ++i) {
            if (curr->children[i]) {
                stack.push_back(curr->children[i]);
            }
        }
        delete curr;
    }
}

TrieNode* CustomTrie::cloneSubtree(const TrieNode* node, std::size_t& outNodeCount) {
    if (!node) return nullptr;

    TrieNode* copy = new TrieNode();
    ++outNodeCount;
    copy->isEndOfWord = node->isEndOfWord;
    copy->searchFrequency = node->searchFrequency;

    for (std::size_t i = 0; i < ALPHABET_SIZE; ++i) {
        if (node->children[i]) {
            copy->children[i] = cloneSubtree(node->children[i], outNodeCount);
        }
    }
    return copy;
}

} // namespace sse::dsa
