#pragma once

#include <string_view>
#include <string>
#include "dsa/CustomDynamicArray.hpp"
#include "indexing/Token.hpp"
#include "indexing/TokenizerConfig.hpp"
#include "indexing/TextNormalizer.hpp"

namespace sse::indexing {

/**
 * @brief High-performance lexical tokenizer producing normalized Token streams.
 */
class Tokenizer {
public:
    explicit Tokenizer(TokenizerConfig config = TokenizerConfig{});
    ~Tokenizer() = default;

    /**
     * @brief Tokenizes input text into a sequence of normalized Token objects.
     * @param text Raw source text (UTF-8 encoded string_view).
     * @param fieldMask Field origin bitmask (FIELD_FILENAME = 0x01, FIELD_CONTENT = 0x02).
     * @return Ordered array of tokens with assigned sequential positions.
     */
    dsa::CustomDynamicArray<Token> tokenize(std::string_view text, uint8_t fieldMask = FIELD_CONTENT) const;

    /**
     * @brief Accesses current tokenizer configuration.
     */
    const TokenizerConfig& config() const noexcept { return m_config; }
    TokenizerConfig& mutableConfig() noexcept { return m_config; }

private:
    TokenizerConfig m_config;
};

} // namespace sse::indexing
