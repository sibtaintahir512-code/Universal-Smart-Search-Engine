#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <cstddef>
#include "indexing/TokenizerConfig.hpp"

namespace sse::indexing {

/**
 * @brief Deterministic text normalization utilities shared by indexing and future query tokenization.
 */
class TextNormalizer {
public:
    /**
     * @brief Converts ASCII uppercase characters [A-Z] to lowercase [a-z].
     * Leaves UTF-8 multi-byte bytes (0x80..0xFF) completely unchanged.
     */
    static std::string toLowerAscii(std::string_view input);

    /**
     * @brief Verifies whether the input byte stream conforms to RFC 3629 standard UTF-8.
     * Rejects overlong encodings, invalid lead bytes, truncated sequences, and surrogate halves.
     */
    static bool isValidUtf8(std::string_view input) noexcept;

    /**
     * @brief Returns length in bytes (1 to 4) of a UTF-8 character given its leading byte.
     * Returns 0 for invalid lead bytes.
     */
    static std::size_t utf8SequenceLength(unsigned char leadByte) noexcept;

    /**
     * @brief Checks if an ASCII character is a word delimiter (whitespace or punctuation).
     */
    static bool isDelimiter(char c) noexcept;

    /**
     * @brief Checks if a character is an ASCII alphanumeric [a-z, A-Z, 0-9].
     */
    static bool isAsciiAlphanumeric(char c) noexcept;

    /**
     * @brief Checks if a character is an ASCII punctuation mark.
     */
    static bool isAsciiPunctuation(char c) noexcept;

    /**
     * @brief Normalizes a single raw token candidate:
     * - Case folds if configured
     * - Verifies UTF-8 integrity
     * - Validates token length constraints
     * Returns empty string if invalid or rejected.
     */
    static std::string normalizeToken(std::string_view rawToken, const TokenizerConfig& config);
};

} // namespace sse::indexing
