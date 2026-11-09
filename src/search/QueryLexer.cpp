#include "search/QueryLexer.hpp"
#include "indexing/TextNormalizer.hpp"
#include <cctype>

namespace sse::search {

bool QueryLexer::isWhitespace(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool QueryLexer::isDelimiter(char c) noexcept {
    return isWhitespace(c) || c == '(' || c == ')';
}

std::string QueryLexer::toUpperAscii(std::string_view s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        if (c >= 'a' && c <= 'z') {
            result.push_back(static_cast<char>(c - 32));
        } else {
            result.push_back(c);
        }
    }
    return result;
}

LexResult QueryLexer::tokenize(std::string_view query) const {
    LexResult res;

    // Check maximum raw query length
    if (query.size() > m_config.maxQueryLength) {
        res.success = false;
        res.errorMessage = "Query length (" + std::to_string(query.size()) +
                           " bytes) exceeds maximum limit of " +
                           std::to_string(m_config.maxQueryLength);
        res.errorOffset = m_config.maxQueryLength;
        return res;
    }

    // RFC 3629 UTF-8 Validation
    if (!indexing::TextNormalizer::isValidUtf8(query)) {
        res.success = false;
        res.errorMessage = "Query contains invalid or malformed UTF-8 byte sequences";
        res.errorOffset = 0;
        return res;
    }

    std::size_t i = 0;
    std::size_t tokenPos = 0;
    const std::size_t len = query.size();

    while (i < len) {
        // 1. Skip whitespace
        while (i < len && isWhitespace(query[i])) {
            ++i;
        }
        if (i >= len) break;

        const std::size_t tokenStart = i;
        const char c = query[i];

        // 2. Reject explicitly unsupported operators/syntax per specification
        if (c == '*' || c == '?') {
            res.success = false;
            res.errorMessage = "Wildcard operators ('*', '?') are unsupported in Phase 6";
            res.errorOffset = tokenStart;
            return res;
        }
        if (c == '~') {
            res.success = false;
            res.errorMessage = "Fuzzy search operator ('~') is unsupported in Phase 6";
            res.errorOffset = tokenStart;
            return res;
        }
        if (c == '^') {
            res.success = false;
            res.errorMessage = "Proximity/boost operator ('^') is unsupported in Phase 6";
            res.errorOffset = tokenStart;
            return res;
        }

        // 3. Parentheses delimiters
        if (c == '(') {
            res.tokens.push_back(QueryToken(QueryTokenType::LeftParen, "(", "(",
                                            FieldQualifier::Unspecified, tokenPos++, tokenStart, 1));
            ++i;
            continue;
        }
        if (c == ')') {
            res.tokens.push_back(QueryToken(QueryTokenType::RightParen, ")", ")",
                                            FieldQualifier::Unspecified, tokenPos++, tokenStart, 1));
            ++i;
            continue;
        }

        // 4. Scan term or qualified token until delimiter
        while (i < len && !isDelimiter(query[i])) {
            char ch = query[i];
            if (ch == '*' || ch == '?') {
                res.success = false;
                res.errorMessage = "Wildcard operators ('*', '?') are unsupported in Phase 6";
                res.errorOffset = i;
                return res;
            }
            if (ch == '~') {
                res.success = false;
                res.errorMessage = "Fuzzy search operator ('~') is unsupported in Phase 6";
                res.errorOffset = i;
                return res;
            }
            if (ch == '^') {
                res.success = false;
                res.errorMessage = "Proximity/boost operator ('^') is unsupported in Phase 6";
                res.errorOffset = i;
                return res;
            }

            // UTF-8 sequence handling
            unsigned char uc = static_cast<unsigned char>(ch);
            if (uc >= 0x80) {
                std::size_t seqLen = indexing::TextNormalizer::utf8SequenceLength(uc);
                if (seqLen == 0 || i + seqLen > len) {
                    res.success = false;
                    res.errorMessage = "Malformed UTF-8 sequence in query";
                    res.errorOffset = i;
                    return res;
                }
                i += seqLen;
            } else {
                ++i;
            }
        }

        std::string_view wordSlice = query.substr(tokenStart, i - tokenStart);
        if (wordSlice.empty()) continue;

        // Check for field qualifier prefix (e.g. "name:report" or "body:report")
        std::size_t colonPos = wordSlice.find(':');
        if (colonPos != std::string_view::npos) {
            std::string_view prefix = wordSlice.substr(0, colonPos);
            std::string_view valPart = wordSlice.substr(colonPos + 1);

            std::string upperPrefix = toUpperAscii(prefix);
            FieldQualifier field = FieldQualifier::Unspecified;

            if (upperPrefix == "NAME") {
                field = FieldQualifier::Filename;
            } else if (upperPrefix == "BODY") {
                field = FieldQualifier::Body;
            } else {
                res.success = false;
                res.errorMessage = "Unsupported or invalid field qualifier '" +
                                   std::string(prefix) + ":' at offset " + std::to_string(tokenStart);
                res.errorOffset = tokenStart;
                return res;
            }

            if (valPart.empty()) {
                res.success = false;
                res.errorMessage = "Missing field qualifier value after '" +
                                   std::string(prefix) + ":' at offset " + std::to_string(tokenStart);
                res.errorOffset = tokenStart;
                return res;
            }

            std::string normTerm = indexing::TextNormalizer::toLowerAscii(valPart);
            res.tokens.push_back(QueryToken(QueryTokenType::Term, normTerm,
                                            std::string(wordSlice), field, tokenPos++,
                                            tokenStart, wordSlice.size()));
        } else {
            // Unqualified term or Boolean operator
            std::string upperWord = toUpperAscii(wordSlice);
            if (upperWord == "AND") {
                res.tokens.push_back(QueryToken(QueryTokenType::And, "AND",
                                                std::string(wordSlice), FieldQualifier::Unspecified,
                                                tokenPos++, tokenStart, wordSlice.size()));
            } else if (upperWord == "OR") {
                res.tokens.push_back(QueryToken(QueryTokenType::Or, "OR",
                                                std::string(wordSlice), FieldQualifier::Unspecified,
                                                tokenPos++, tokenStart, wordSlice.size()));
            } else if (upperWord == "NOT") {
                res.tokens.push_back(QueryToken(QueryTokenType::Not, "NOT",
                                                std::string(wordSlice), FieldQualifier::Unspecified,
                                                tokenPos++, tokenStart, wordSlice.size()));
            } else {
                std::string normTerm = indexing::TextNormalizer::toLowerAscii(wordSlice);
                res.tokens.push_back(QueryToken(QueryTokenType::Term, normTerm,
                                                std::string(wordSlice), FieldQualifier::Unspecified,
                                                tokenPos++, tokenStart, wordSlice.size()));
            }
        }

        // Check token count limit
        if (res.tokens.size() > m_config.maxQueryTokens) {
            res.success = false;
            res.errorMessage = "Query exceeds maximum token limit of " +
                               std::to_string(m_config.maxQueryTokens);
            res.errorOffset = tokenStart;
            return res;
        }
    }

    // Append EndOfInput sentinel
    res.tokens.push_back(QueryToken(QueryTokenType::EndOfInput, "", "",
                                    FieldQualifier::Unspecified, tokenPos, len, 0));
    res.success = true;
    return res;
}

} // namespace sse::search
