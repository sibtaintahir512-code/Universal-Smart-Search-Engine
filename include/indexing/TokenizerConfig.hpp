#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <fstream>
#include <unordered_set>

namespace sse::indexing {

/**
 * @brief Configuration parameters for text tokenization and normalization.
 */
struct TokenizerConfig {
    std::size_t minTokenLength{1};  // Minimum character count for indexed tokens
    std::size_t maxTokenLength{64}; // Maximum character count (tokens exceeding this are skipped)
    bool        caseFold{true};     // Convert ASCII [A-Z] to [a-z]
    bool        filterStopWords{false}; // Disabled by default per specification
    bool        preserveNumbers{true};  // Preserve numeric digits in tokens
    bool        stripPunctuation{true}; // Treat punctuation marks as delimiters

    // In-memory set of stop words for fast O(1) membership testing
    std::unordered_set<std::string> stopWords;

    TokenizerConfig() = default;

    /**
     * @brief Adds a normalized word to the stop-word filter list.
     */
    void addStopWord(std::string word) {
        if (!word.empty()) {
            for (char& c : word) {
                if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
            }
            stopWords.insert(std::move(word));
        }
    }

    /**
     * @brief Loads line-delimited stop-words from a text file (e.g. resources/stopwords/stopwords.txt).
     * @return true if file opened and words loaded, false otherwise.
     */
    bool loadStopWords(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            // Trim trailing CR/whitespace
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
                line.pop_back();
            }
            // Trim leading whitespace
            std::size_t start = 0;
            while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) {
                ++start;
            }
            if (start < line.size()) {
                addStopWord(line.substr(start));
            }
        }
        return !stopWords.empty();
    }

    /**
     * @brief Checks if a normalized word is registered as a stop word.
     */
    bool isStopWord(const std::string& word) const noexcept {
        if (!filterStopWords || stopWords.empty()) {
            return false;
        }
        return stopWords.find(word) != stopWords.end();
    }
};

} // namespace sse::indexing
