#include "indexing/Tokenizer.hpp"

namespace sse::indexing {

Tokenizer::Tokenizer(TokenizerConfig config)
    : m_config(std::move(config)) {}

dsa::CustomDynamicArray<Token> Tokenizer::tokenize(std::string_view text, uint8_t fieldMask) const {
    dsa::CustomDynamicArray<Token> tokens;
    if (text.empty()) {
        return tokens;
    }

    std::size_t i = 0;
    const std::size_t n = text.size();
    uint32_t currentPosition = 0;

    while (i < n) {
        // Skip leading delimiters
        while (i < n) {
            auto b = static_cast<unsigned char>(text[i]);
            if (b < 0x80) {
                if (TextNormalizer::isDelimiter(static_cast<char>(b))) {
                    ++i;
                    continue;
                }
            }
            break;
        }

        if (i >= n) break;

        std::size_t tokenStart = i;
        std::string currentRaw;

        // Accumulate token characters until next delimiter or EOF
        while (i < n) {
            auto b = static_cast<unsigned char>(text[i]);
            if (b < 0x80) {
                char c = static_cast<char>(b);
                if (TextNormalizer::isDelimiter(c)) {
                    break;
                }
                currentRaw.push_back(c);
                ++i;
            } else {
                // Multi-byte UTF-8 character
                std::size_t seqLen = TextNormalizer::utf8SequenceLength(b);
                if (seqLen == 0 || i + seqLen > n) {
                    // Invalid lead byte or truncated sequence; skip byte safely
                    ++i;
                    continue;
                }
                std::string_view seq = text.substr(i, seqLen);
                if (TextNormalizer::isValidUtf8(seq)) {
                    currentRaw.append(seq.data(), seqLen);
                    i += seqLen;
                } else {
                    ++i;
                }
            }
        }

        if (!currentRaw.empty()) {
            uint32_t pos = currentPosition++;
            std::string normalized = TextNormalizer::normalizeToken(currentRaw, m_config);
            if (!normalized.empty()) {
                tokens.push_back(Token(std::move(normalized), pos, fieldMask, static_cast<uint32_t>(tokenStart)));
            }
        }
    }

    return tokens;
}

} // namespace sse::indexing
