#include "TestHarness.hpp"
#include <string>
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include "search/QueryLexer.hpp"
#include "search/QueryParser.hpp"
#include "search/QueryEvaluator.hpp"

namespace sse::test {

static void indexDocument(indexing::DocumentCatalog& catalog,
                          indexing::InvertedIndex& index,
                          DocID expectedId,
                          const std::string& name,
                          const std::string& bodyTerms) {
    FileMetadata meta;
    meta.category = FileCategory::Documents;
    meta.fileSize = 1024;
    meta.extractionState = ExtractionStatus::Extracted;

    DocID docId = catalog.registerDocument(meta, "/test/" + name, name);
    TEST_ASSERT_EQ(docId, expectedId);

    // Ingest terms from body
    std::string term;
    for (char c : bodyTerms) {
        if (c == ' ') {
            if (!term.empty()) {
                index.addTerm(term, docId, 0, 1, FIELD_CONTENT);
                term.clear();
            }
        } else {
            term.push_back(c);
        }
    }
    if (!term.empty()) {
        index.addTerm(term, docId, 0, 1, FIELD_CONTENT);
    }
}

void runBooleanEvaluatorTests() {
    std::cout << "[RUNNING] BooleanEvaluator Tests..." << std::endl;

    using namespace sse::indexing;
    using namespace sse::search;

    DocumentCatalog catalog;
    InvertedIndex index;

    // Build Task L3 controlled test corpus
    // Document 1: apple banana
    // Document 2: apple orange
    // Document 3: banana orange
    // Document 4: apple banana orange
    // Document 5: grape
    indexDocument(catalog, index, 1, "doc1.txt", "apple banana");
    indexDocument(catalog, index, 2, "doc2.txt", "apple orange");
    indexDocument(catalog, index, 3, "doc3.txt", "banana orange");
    indexDocument(catalog, index, 4, "doc4.txt", "apple banana orange");
    indexDocument(catalog, index, 5, "doc5.txt", "grape");

    QueryLexer lexer;
    QueryParser parser;
    QueryEvaluator evaluator(&index, &catalog);

    auto evalQuery = [&](const std::string& q) -> ResultSet {
        LexResult lex = lexer.tokenize(q);
        TEST_ASSERT(lex.success);
        ParseResult parse = parser.parse(lex.tokens);
        TEST_ASSERT(parse.success);
        EvaluationResult eval = evaluator.evaluate(parse.root.get());
        TEST_ASSERT(eval.success);
        return eval.resultSet;
    };

    // 1. Single Term: "apple" -> {1, 2, 4}
    {
        ResultSet res = evalQuery("apple");
        TEST_ASSERT_EQ(res.size(), 3);
        TEST_ASSERT_EQ(res[0], 1);
        TEST_ASSERT_EQ(res[1], 2);
        TEST_ASSERT_EQ(res[2], 4);
    }

    // 2. Conjunction: "apple AND banana" -> {1, 4}
    {
        ResultSet res = evalQuery("apple AND banana");
        TEST_ASSERT_EQ(res.size(), 2);
        TEST_ASSERT_EQ(res[0], 1);
        TEST_ASSERT_EQ(res[1], 4);
    }

    // 3. Disjunction: "apple OR banana" -> {1, 2, 3, 4}
    {
        ResultSet res = evalQuery("apple OR banana");
        TEST_ASSERT_EQ(res.size(), 4);
        TEST_ASSERT_EQ(res[0], 1);
        TEST_ASSERT_EQ(res[1], 2);
        TEST_ASSERT_EQ(res[2], 3);
        TEST_ASSERT_EQ(res[3], 4);
    }

    // 4. Conjunction with Negation: "apple AND NOT banana" -> {2}
    {
        ResultSet res = evalQuery("apple AND NOT banana");
        TEST_ASSERT_EQ(res.size(), 1);
        TEST_ASSERT_EQ(res[0], 2);
    }

    // 5. Unary Negation: "NOT apple" -> {3, 5}
    {
        ResultSet res = evalQuery("NOT apple");
        TEST_ASSERT_EQ(res.size(), 2);
        TEST_ASSERT_EQ(res[0], 3);
        TEST_ASSERT_EQ(res[1], 5);
    }

    // 6. Grouped Expression: "(apple OR banana) AND orange" -> {2, 3, 4}
    {
        ResultSet res = evalQuery("(apple OR banana) AND orange");
        TEST_ASSERT_EQ(res.size(), 3);
        TEST_ASSERT_EQ(res[0], 2);
        TEST_ASSERT_EQ(res[1], 3);
        TEST_ASSERT_EQ(res[2], 4);
    }

    // 7. Precedence Expression: "apple OR banana AND orange" -> {1, 2, 3, 4}
    {
        ResultSet res = evalQuery("apple OR banana AND orange");
        TEST_ASSERT_EQ(res.size(), 4);
        TEST_ASSERT_EQ(res[0], 1);
        TEST_ASSERT_EQ(res[1], 2);
        TEST_ASSERT_EQ(res[2], 3);
        TEST_ASSERT_EQ(res[3], 4);
    }

    // 8. Non-existent Term Query
    {
        ResultSet res = evalQuery("nonexistent");
        TEST_ASSERT(res.empty());

        ResultSet res2 = evalQuery("apple AND nonexistent");
        TEST_ASSERT(res2.empty());

        ResultSet res3 = evalQuery("apple OR nonexistent");
        TEST_ASSERT_EQ(res3.size(), 3); // identical to apple: 1, 2, 4
    }

    // 9. Negation of Universe
    {
        ResultSet res = evalQuery("NOT (apple OR banana OR grape)");
        TEST_ASSERT(res.empty()); // All 5 docs contain either apple, banana, or grape
    }
}

} // namespace sse::test
