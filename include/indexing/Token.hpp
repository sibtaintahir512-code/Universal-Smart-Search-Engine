#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include "core/CommonTypes.hpp"

namespace sse::indexing {

/**
 * @brief Discrete lexical token extracted from document text or filename.
 */
struct Token {
    std::string term;        // Normalized lowercase token string
    uint32_t    position{0}; // 0-based sequential token index in the field
    uint8_t     field{FIELD_CONTENT}; // FIELD_FILENAME (0x01) or FIELD_CONTENT (0x02)
    uint32_t    charOffset{0}; // Byte offset in original source string

    Token() = default;

    Token(std::string t, uint32_t pos, uint8_t f = FIELD_CONTENT, uint32_t offset = 0)
        : term(std::move(t)), position(pos), field(f), charOffset(offset) {}

    bool operator==(const Token& other) const noexcept {
        return position == other.position &&
               field == other.field &&
               charOffset == other.charOffset &&
               term == other.term;
    }

    bool operator!=(const Token& other) const noexcept {
        return !(*this == other);
    }
};

} // namespace sse::indexing
