#include "TestHarness.hpp"
#include "search/QueryLexer.hpp"
#include "search/QueryParser.hpp"
#include <string>

namespace sse::test {

void runQueryParserTests() {
    std::cout << "[RUNNING] QueryParser Tests..." << std::endl;

    using namespace sse::search;
    QueryLexer lexer;
    QueryParser parser;

    // 1. Single Term Expression
    {
        LexResult lex = lexer.tokenize("computer");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.root != nullptr);
        TEST_ASSERT(res.root->type() == QueryNodeType::Term);
        TEST_ASSERT_EQ(res.root->depth(), 1);
        TEST_ASSERT_EQ(res.root->nodeCount(), 1);
        auto* termNode = static_cast<TermNode*>(res.root.get());
        TEST_ASSERT_EQ(termNode->term, std::string("computer"));
        TEST_ASSERT(termNode->field == FieldQualifier::Unspecified);
    }

    // 2. Binary AND Expression
    {
        LexResult lex = lexer.tokenize("computer AND science");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.root->type() == QueryNodeType::And);
        TEST_ASSERT_EQ(res.root->nodeCount(), 3);
        auto* andNode = static_cast<AndNode*>(res.root.get());
        TEST_ASSERT(!andNode->isImplicit);
        TEST_ASSERT(andNode->left->type() == QueryNodeType::Term);
        TEST_ASSERT(andNode->right->type() == QueryNodeType::Term);
    }

    // 3. Binary OR Expression
    {
        LexResult lex = lexer.tokenize("apple OR banana");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.root->type() == QueryNodeType::Or);
        TEST_ASSERT_EQ(res.root->nodeCount(), 3);
        auto* orNode = static_cast<OrNode*>(res.root.get());
        TEST_ASSERT(orNode->left->type() == QueryNodeType::Term);
        TEST_ASSERT(orNode->right->type() == QueryNodeType::Term);
    }

    // 4. Unary NOT Expression
    {
        LexResult lex = lexer.tokenize("NOT obsolete");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.root->type() == QueryNodeType::Not);
        TEST_ASSERT_EQ(res.root->nodeCount(), 2);
        auto* notNode = static_cast<NotNode*>(res.root.get());
        TEST_ASSERT(notNode->operand->type() == QueryNodeType::Term);
    }

    // 5. Implicit AND Insertion between Adjacent Terms
    {
        LexResult lex = lexer.tokenize("computer science");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.root->type() == QueryNodeType::And);
        auto* andNode = static_cast<AndNode*>(res.root.get());
        TEST_ASSERT(andNode->isImplicit);
        TEST_ASSERT(andNode->left->type() == QueryNodeType::Term);
        TEST_ASSERT(andNode->right->type() == QueryNodeType::Term);
    }

    // 6. Implicit AND with Parentheses
    {
        LexResult lex = lexer.tokenize("(apple OR banana) orange");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.root->type() == QueryNodeType::And);
        auto* andNode = static_cast<AndNode*>(res.root.get());
        TEST_ASSERT(andNode->isImplicit);
        TEST_ASSERT(andNode->left->type() == QueryNodeType::Or);
        TEST_ASSERT(andNode->right->type() == QueryNodeType::Term);
    }

    // 7. Implicit AND with NOT
    {
        LexResult lex = lexer.tokenize("apple NOT banana");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.root->type() == QueryNodeType::And);
        auto* andNode = static_cast<AndNode*>(res.root.get());
        TEST_ASSERT(andNode->isImplicit);
        TEST_ASSERT(andNode->left->type() == QueryNodeType::Term);
        TEST_ASSERT(andNode->right->type() == QueryNodeType::Not);
    }

    // 8. Nested Parentheses
    {
        LexResult lex = lexer.tokenize("((apple))");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.root->type() == QueryNodeType::Term);
    }

    // 9. Error: Empty Query
    {
        LexResult lex = lexer.tokenize("");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(!res.success);
        TEST_ASSERT(res.errorMessage.find("Empty query") != std::string::npos);
    }

    // 10. Error: Missing Right Operand for AND
    {
        LexResult lex = lexer.tokenize("apple AND");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(!res.success);
        TEST_ASSERT(res.errorMessage.find("Missing right operand") != std::string::npos);
    }

    // 11. Error: Missing Right Operand for OR
    {
        LexResult lex = lexer.tokenize("apple OR");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(!res.success);
        TEST_ASSERT(res.errorMessage.find("Missing right operand") != std::string::npos);
    }

    // 12. Error: Missing Operand for NOT
    {
        LexResult lex = lexer.tokenize("NOT");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(!res.success);
        TEST_ASSERT(res.errorMessage.find("Missing operand for 'NOT'") != std::string::npos);
    }

    // 13. Error: Mismatched Opening Parenthesis
    {
        LexResult lex = lexer.tokenize("(apple AND banana");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(!res.success);
        TEST_ASSERT(res.errorMessage.find("Mismatched parentheses") != std::string::npos);
    }

    // 14. Error: Unexpected Closing Parenthesis
    {
        LexResult lex = lexer.tokenize("apple)");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(!res.success);
        TEST_ASSERT(res.errorMessage.find("Unexpected") != std::string::npos);

        LexResult lex2 = lexer.tokenize(")");
        ParseResult res2 = parser.parse(lex2.tokens);
        TEST_ASSERT(!res2.success);
        TEST_ASSERT(res2.errorMessage.find("Unexpected closing parenthesis") != std::string::npos);
    }

    // 15. Error: Consecutive Operators
    {
        LexResult lex1 = lexer.tokenize("apple AND OR banana");
        ParseResult res1 = parser.parse(lex1.tokens);
        TEST_ASSERT(!res1.success);
        TEST_ASSERT(res1.errorMessage.find("Consecutive operators") != std::string::npos);

        LexResult lex2 = lexer.tokenize("apple OR AND banana");
        ParseResult res2 = parser.parse(lex2.tokens);
        TEST_ASSERT(!res2.success);
        TEST_ASSERT(res2.errorMessage.find("Consecutive operators") != std::string::npos);
    }

    // 16. Error: Empty Parentheses
    {
        LexResult lex = lexer.tokenize("apple AND ()");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(!res.success);
        TEST_ASSERT(res.errorMessage.find("Empty parentheses") != std::string::npos);
    }

    // 17. Error: Maximum AST Depth Exceeded
    {
        SearchConfig cfg;
        cfg.maxAstDepth = 3;
        QueryParser limParser(cfg);
        LexResult lex = lexer.tokenize("a AND b AND c AND d");
        ParseResult res = limParser.parse(lex.tokens);
        TEST_ASSERT(!res.success);
        TEST_ASSERT(res.errorMessage.find("exceeds maximum AST depth") != std::string::npos);
    }

    // 18. Error: Maximum AST Node Count Exceeded
    {
        SearchConfig cfg;
        cfg.maxAstNodes = 5;
        QueryParser limParser(cfg);
        LexResult lex = lexer.tokenize("a AND b AND c AND d");
        ParseResult res = limParser.parse(lex.tokens);
        TEST_ASSERT(!res.success);
        TEST_ASSERT(res.errorMessage.find("exceeds maximum AST node limit") != std::string::npos);
    }
}

} // namespace sse::test
