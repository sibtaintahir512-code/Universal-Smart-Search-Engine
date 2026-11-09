#include "indexing/TextNormalizer.hpp"
#include <cctype>

namespace sse::indexing {

std::string TextNormalizer::toLowerAscii(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    for (char c : input) {
        if (c >= 'A' && c <= 'Z') {
            result.push_back(static_cast<char>(c + 32));
        } else {
            result.push_back(c);
        }
    }
    return result;
}

std::size_t TextNormalizer::utf8SequenceLength(unsigned char leadByte) noexcept {
    if ((leadByte & 0x80) == 0x00) return 1; // 0xxxxxxx (ASCII)
    if ((leadByte & 0xE0) == 0xC0) return 2; // 110xxxxx
    if ((leadByte & 0xF0) == 0xE0) return 3; // 1110xxxx
    if ((leadByte & 0xF8) == 0xF0) return 4; // 11110xxx
    return 0; // Invalid UTF-8 lead byte
}

bool TextNormalizer::isValidUtf8(std::string_view input) noexcept {
    std::size_t i = 0;
    const std::size_t n = input.size();

    while (i < n) {
        auto b0 = static_cast<unsigned char>(input[i]);
        if (b0 <= 0x7F) {
            ++i;
            continue;
        }

        if (b0 >= 0xC2 && b0 <= 0xDF) {
            if (i + 1 >= n) return false;
            auto b1 = static_cast<unsigned char>(input[i + 1]);
            if ((b1 & 0xC0) != 0x80) return false;
            i += 2;
        } else if (b0 >= 0xE0 && b0 <= 0xEF) {
            if (i + 2 >= n) return false;
            auto b1 = static_cast<unsigned char>(input[i + 1]);
            auto b2 = static_cast<unsigned char>(input[i + 2]);
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) return false;

            // Reject overlong sequence
            if (b0 == 0xE0 && b1 < 0xA0) return false;
            // Reject UTF-16 surrogates (U+D800 .. U+DFFF)
            if (b0 == 0xED && b1 >= 0xA0) return false;

            i += 3;
        } else if (b0 >= 0xF0 && b0 <= 0xF4) {
            if (i + 3 >= n) return false;
            auto b1 = static_cast<unsigned char>(input[i + 1]);
            auto b2 = static_cast<unsigned char>(input[i + 2]);
            auto b3 = static_cast<unsigned char>(input[i + 3]);
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) return false;

            // Reject overlong sequence
            if (b0 == 0xF0 && b1 < 0x90) return false;
            // Reject codepoints > U+10FFFF
            if (b0 == 0xF4 && b1 > 0x8F) return false;

            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

bool TextNormalizer::isAsciiAlphanumeric(char c) noexcept {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9');
}

bool TextNormalizer::isAsciiPunctuation(char c) noexcept {
    // Standard ASCII punctuation marks
    return (c >= '!' && c <= '/') ||  // ! " # $ % & ' ( ) * + , - . /
           (c >= ':' && c <= '@') ||  // : ; < = > ? @
           (c >= '[' && c <= '`') ||  // [ \ ] ^ _ `
           (c >= '{' && c <= '~');    // { | } ~
}

bool TextNormalizer::isDelimiter(char c) noexcept {
    // Whitespace: space, tab, newline, carriage return, form feed, vertical tab
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') {
        return true;
    }
    return isAsciiPunctuation(c);
}

std::string TextNormalizer::normalizeToken(std::string_view rawToken, const TokenizerConfig& config) {
    if (rawToken.empty()) {
        return "";
    }

    // Validate UTF-8 conformance
    if (!isValidUtf8(rawToken)) {
        return "";
    }

    std::string normalized;
    if (config.caseFold) {
        normalized = toLowerAscii(rawToken);
    } else {
        normalized = std::string(rawToken);
    }

    // Length check
    if (normalized.size() < config.minTokenLength || normalized.size() > config.maxTokenLength) {
        return "";
    }

    // Stop-word check
    if (config.isStopWord(normalized)) {
        return "";
    }

    return normalized;
}

} // namespace sse::indexing
