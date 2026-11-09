#include "TestHarness.hpp"
#include "search/QueryLexer.hpp"
#include <string>

namespace sse::test {

void runQueryLexerTests() {
    std::cout << "[RUNNING] QueryLexer Tests..." << std::endl;

    using namespace sse::search;
    QueryLexer lexer;

    // 1. Single Term
    {
        LexResult res = lexer.tokenize("computer");
        TEST_ASSERT(res.success);
        TEST_ASSERT_EQ(res.tokens.size(), 2); // Term + EndOfInput
        TEST_ASSERT(res.tokens[0].type == QueryTokenType::Term);
        TEST_ASSERT_EQ(res.tokens[0].value, std::string("computer"));
        TEST_ASSERT(res.tokens[0].field == FieldQualifier::Unspecified);
        TEST_ASSERT_EQ(res.tokens[0].charOffset, 0);
        TEST_ASSERT(res.tokens[1].type == QueryTokenType::EndOfInput);
    }

    // 2. Multiple Terms with Whitespace
    {
        LexResult res = lexer.tokenize("   computer    science   ");
        TEST_ASSERT(res.success);
        TEST_ASSERT_EQ(res.tokens.size(), 3); // 2 Terms + EndOfInput
        TEST_ASSERT(res.tokens[0].type == QueryTokenType::Term);
        TEST_ASSERT_EQ(res.tokens[0].value, std::string("computer"));
        TEST_ASSERT(res.tokens[1].type == QueryTokenType::Term);
        TEST_ASSERT_EQ(res.tokens[1].value, std::string("science"));
        TEST_ASSERT(res.tokens[2].type == QueryTokenType::EndOfInput);
    }

    // 3. Case-Insensitive Operators
    {
        LexResult res = lexer.tokenize("apple AND banana or orange Not grape");
        TEST_ASSERT(res.success);
        TEST_ASSERT_EQ(res.tokens.size(), 8);
        TEST_ASSERT_EQ(res.tokens[0].value, std::string("apple"));
        TEST_ASSERT(res.tokens[1].type == QueryTokenType::And);
        TEST_ASSERT_EQ(res.tokens[2].value, std::string("banana"));
        TEST_ASSERT(res.tokens[3].type == QueryTokenType::Or);
        TEST_ASSERT_EQ(res.tokens[4].value, std::string("orange"));
        TEST_ASSERT(res.tokens[5].type == QueryTokenType::Not);
        TEST_ASSERT_EQ(res.tokens[6].value, std::string("grape"));
        TEST_ASSERT(res.tokens[7].type == QueryTokenType::EndOfInput);
    }

    // 4. Parentheses
    {
        LexResult res = lexer.tokenize("(apple OR banana) AND orange");
        TEST_ASSERT(res.success);
        TEST_ASSERT_EQ(res.tokens.size(), 8);
        TEST_ASSERT(res.tokens[0].type == QueryTokenType::LeftParen);
        TEST_ASSERT_EQ(res.tokens[1].value, std::string("apple"));
        TEST_ASSERT(res.tokens[2].type == QueryTokenType::Or);
        TEST_ASSERT_EQ(res.tokens[3].value, std::string("banana"));
        TEST_ASSERT(res.tokens[4].type == QueryTokenType::RightParen);
        TEST_ASSERT(res.tokens[5].type == QueryTokenType::And);
        TEST_ASSERT_EQ(res.tokens[6].value, std::string("orange"));
        TEST_ASSERT(res.tokens[7].type == QueryTokenType::EndOfInput);
    }

    // 5. Field Qualifiers: name: and body:
    {
        LexResult res = lexer.tokenize("name:report AND body:finance");
        TEST_ASSERT(res.success);
        TEST_ASSERT_EQ(res.tokens.size(), 4);
        TEST_ASSERT(res.tokens[0].type == QueryTokenType::Term);
        TEST_ASSERT_EQ(res.tokens[0].value, std::string("report"));
        TEST_ASSERT(res.tokens[0].field == FieldQualifier::Filename);
        TEST_ASSERT(res.tokens[1].type == QueryTokenType::And);
        TEST_ASSERT(res.tokens[2].type == QueryTokenType::Term);
        TEST_ASSERT_EQ(res.tokens[2].value, std::string("finance"));
        TEST_ASSERT(res.tokens[2].field == FieldQualifier::Body);
    }

    // 6. Case-Insensitive Field Qualifiers
    {
        LexResult res = lexer.tokenize("NAME:invoice BODY:payment");
        TEST_ASSERT(res.success);
        TEST_ASSERT_EQ(res.tokens.size(), 3);
        TEST_ASSERT(res.tokens[0].field == FieldQualifier::Filename);
        TEST_ASSERT_EQ(res.tokens[0].value, std::string("invoice"));
        TEST_ASSERT(res.tokens[1].field == FieldQualifier::Body);
        TEST_ASSERT_EQ(res.tokens[1].value, std::string("payment"));
    }

    // 7. Missing Field Qualifier Value
    {
        LexResult res1 = lexer.tokenize("name:");
        TEST_ASSERT(!res1.success);
        TEST_ASSERT(res1.errorMessage.find("Missing field qualifier value") != std::string::npos);

        LexResult res2 = lexer.tokenize("name:   ");
        TEST_ASSERT(!res2.success);
    }

    // 8. Unsupported Field Qualifier
    {
        LexResult res = lexer.tokenize("author:john");
        TEST_ASSERT(!res.success);
        TEST_ASSERT(res.errorMessage.find("Unsupported or invalid field qualifier") != std::string::npos);
    }

    // 9. Rejection of Unsupported Syntax: Wildcards (*, ?)
    {
        LexResult res1 = lexer.tokenize("comp*");
        TEST_ASSERT(!res1.success);
        TEST_ASSERT(res1.errorMessage.find("Wildcard") != std::string::npos);

        LexResult res2 = lexer.tokenize("te?t");
        TEST_ASSERT(!res2.success);
        TEST_ASSERT(res2.errorMessage.find("Wildcard") != std::string::npos);
    }

    // 10. Rejection of Unsupported Syntax: Fuzzy Search (~)
    {
        LexResult res = lexer.tokenize("roam~2");
        TEST_ASSERT(!res.success);
        TEST_ASSERT(res.errorMessage.find("Fuzzy") != std::string::npos);
    }

    // 11. Rejection of Unsupported Syntax: Proximity (^)
    {
        LexResult res = lexer.tokenize("apple^4");
        TEST_ASSERT(!res.success);
        TEST_ASSERT(res.errorMessage.find("Proximity") != std::string::npos);
    }

    // 12. Empty and Whitespace-Only Input
    {
        LexResult res1 = lexer.tokenize("");
        TEST_ASSERT(res1.success);
        TEST_ASSERT_EQ(res1.tokens.size(), 1);
        TEST_ASSERT(res1.tokens[0].type == QueryTokenType::EndOfInput);

        LexResult res2 = lexer.tokenize("    \t  \n  ");
        TEST_ASSERT(res2.success);
        TEST_ASSERT_EQ(res2.tokens.size(), 1);
        TEST_ASSERT(res2.tokens[0].type == QueryTokenType::EndOfInput);
    }

    // 13. Invalid UTF-8 Sequence Rejection
    {
        const char badBytes[] = {'f', 'i', 'l', 'e', ' ', static_cast<char>(0xFF), static_cast<char>(0xFE), '\0'};
        LexResult res = lexer.tokenize(std::string_view(badBytes, 7));
        TEST_ASSERT(!res.success);
        TEST_ASSERT(res.errorMessage.find("UTF-8") != std::string::npos);
    }

    // 14. Query Length Limit
    {
        SearchConfig cfg;
        cfg.maxQueryLength = 10;
        QueryLexer limLexer(cfg);
        LexResult res = limLexer.tokenize("this query is too long");
        TEST_ASSERT(!res.success);
        TEST_ASSERT(res.errorMessage.find("exceeds maximum limit") != std::string::npos);
    }

    // 15. Token Count Limit
    {
        SearchConfig cfg;
        cfg.maxQueryTokens = 3;
        QueryLexer limLexer(cfg);
        LexResult res = limLexer.tokenize("one two three four five");
        TEST_ASSERT(!res.success);
        TEST_ASSERT(res.errorMessage.find("maximum token limit") != std::string::npos);
    }
}

} // namespace sse::test
