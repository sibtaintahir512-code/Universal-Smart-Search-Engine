#include "TestHarness.hpp"
#include "search/QueryLexer.hpp"
#include "search/QueryParser.hpp"
#include <string>

namespace sse::test {

void runQueryPrecedenceTests() {
    std::cout << "[RUNNING] QueryPrecedence Tests..." << std::endl;

    using namespace sse::search;
    QueryLexer lexer;
    QueryParser parser;

    // 1. AND Precedes OR: A OR B AND C -> A OR (B AND C)
    {
        LexResult lex = lexer.tokenize("computer OR programming AND science");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.root->type() == QueryNodeType::Or);
        auto* orNode = static_cast<OrNode*>(res.root.get());
        TEST_ASSERT(orNode->left->type() == QueryNodeType::Term);
        TEST_ASSERT_EQ(static_cast<TermNode*>(orNode->left.get())->term, std::string("computer"));
        TEST_ASSERT(orNode->right->type() == QueryNodeType::And);
        auto* andNode = static_cast<AndNode*>(orNode->right.get());
        TEST_ASSERT_EQ(static_cast<TermNode*>(andNode->left.get())->term, std::string("programming"));
        TEST_ASSERT_EQ(static_cast<TermNode*>(andNode->right.get())->term, std::string("science"));
    }

    // 2. Parentheses Override Precedence: (A OR B) AND C
    {
        LexResult lex = lexer.tokenize("(computer OR programming) AND science");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.root->type() == QueryNodeType::And);
        auto* andNode = static_cast<AndNode*>(res.root.get());
        TEST_ASSERT(andNode->left->type() == QueryNodeType::Or);
        TEST_ASSERT(andNode->right->type() == QueryNodeType::Term);
        TEST_ASSERT_EQ(static_cast<TermNode*>(andNode->right.get())->term, std::string("science"));
    }

    // 3. AND Before OR: A AND B OR C -> (A AND B) OR C
    {
        LexResult lex = lexer.tokenize("apple AND banana OR orange");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.root->type() == QueryNodeType::Or);
        auto* orNode = static_cast<OrNode*>(res.root.get());
        TEST_ASSERT(orNode->left->type() == QueryNodeType::And);
        TEST_ASSERT(orNode->right->type() == QueryNodeType::Term);
    }

    // 4. NOT Precedes AND: NOT A AND B -> (NOT A) AND B
    {
        LexResult lex = lexer.tokenize("NOT apple AND banana");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.root->type() == QueryNodeType::And);
        auto* andNode = static_cast<AndNode*>(res.root.get());
        TEST_ASSERT(andNode->left->type() == QueryNodeType::Not);
        TEST_ASSERT(andNode->right->type() == QueryNodeType::Term);
        TEST_ASSERT_EQ(static_cast<TermNode*>(andNode->right.get())->term, std::string("banana"));
    }

    // 5. NOT with Parentheses: NOT (A OR B)
    {
        LexResult lex = lexer.tokenize("NOT (apple OR banana)");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.root->type() == QueryNodeType::Not);
        auto* notNode = static_cast<NotNode*>(res.root.get());
        TEST_ASSERT(notNode->operand->type() == QueryNodeType::Or);
    }

    // 6. Complex Precedence: (A AND B) OR (C AND NOT D)
    {
        LexResult lex = lexer.tokenize("(apple AND banana) OR (orange AND NOT grape)");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.root->type() == QueryNodeType::Or);
        auto* orNode = static_cast<OrNode*>(res.root.get());

        TEST_ASSERT(orNode->left->type() == QueryNodeType::And);
        TEST_ASSERT(orNode->right->type() == QueryNodeType::And);
        auto* rightAnd = static_cast<AndNode*>(orNode->right.get());
        TEST_ASSERT(rightAnd->left->type() == QueryNodeType::Term);
        TEST_ASSERT(rightAnd->right->type() == QueryNodeType::Not);
    }

    // 7. Left Associativity of OR: A OR B OR C -> ((A OR B) OR C)
    {
        LexResult lex = lexer.tokenize("a OR b OR c");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.root->type() == QueryNodeType::Or);
        auto* orNode = static_cast<OrNode*>(res.root.get());
        TEST_ASSERT(orNode->left->type() == QueryNodeType::Or);
        TEST_ASSERT(orNode->right->type() == QueryNodeType::Term);
    }

    // 8. Left Associativity of AND: A AND B AND C -> ((A AND B) AND C)
    {
        LexResult lex = lexer.tokenize("a AND b AND c");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.root->type() == QueryNodeType::And);
        auto* andNode = static_cast<AndNode*>(res.root.get());
        TEST_ASSERT(andNode->left->type() == QueryNodeType::And);
        TEST_ASSERT(andNode->right->type() == QueryNodeType::Term);
    }

    // 9. Mixed Implicit AND and OR: A OR B C -> A OR (B [AND] C)
    {
        LexResult lex = lexer.tokenize("apple OR banana orange");
        ParseResult res = parser.parse(lex.tokens);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.root->type() == QueryNodeType::Or);
        auto* orNode = static_cast<OrNode*>(res.root.get());
        TEST_ASSERT(orNode->left->type() == QueryNodeType::Term);
        TEST_ASSERT(orNode->right->type() == QueryNodeType::And);
        auto* andNode = static_cast<AndNode*>(orNode->right.get());
        TEST_ASSERT(andNode->isImplicit);
    }
}

} // namespace sse::test
