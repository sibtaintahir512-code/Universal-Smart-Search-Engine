#include "TestHarness.hpp"
#include <string>
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include "search/QueryLexer.hpp"
#include "search/QueryParser.hpp"
#include "search/QueryEvaluator.hpp"

namespace sse::test {

void runFieldSearchTests() {
    std::cout << "[RUNNING] FieldSearch Tests..." << std::endl;

    using namespace sse::indexing;
    using namespace sse::search;

    DocumentCatalog catalog;
    InvertedIndex index;

    auto setupDoc = [&](DocID id, const std::string& name, const std::string& body) {
        FileMetadata meta;
        meta.category = FileCategory::Documents;
        meta.fileSize = 2048;
        meta.extractionState = ExtractionStatus::Extracted;

        DocID dId = catalog.registerDocument(meta, "/data/" + name, name);
        TEST_ASSERT_EQ(dId, id);

        // Ingest filename term (strip extension for simplicity)
        std::string fnameTerm = name.substr(0, name.find('.'));
        index.addTerm(fnameTerm, dId, 1, 0, FIELD_FILENAME);

        // Ingest body terms
        std::string term;
        for (char c : body) {
            if (c == ' ') {
                if (!term.empty()) {
                    index.addTerm(term, dId, 0, 1, FIELD_CONTENT);
                    term.clear();
                }
            } else {
                term.push_back(c);
            }
        }
        if (!term.empty()) {
            index.addTerm(term, dId, 0, 1, FIELD_CONTENT);
        }
    };

    setupDoc(1, "report.pdf", "financial statements and tax audit");
    setupDoc(2, "budget.xlsx", "annual report and revenue targets");
    setupDoc(3, "invoice.docx", "payment details and bank transaction");
    setupDoc(4, "temporary.txt", "temporary log dump");

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

    // 1. Unqualified Search for "report": matches Doc 1 (in name) and Doc 2 (in body)
    {
        ResultSet res = evalQuery("report");
        TEST_ASSERT_EQ(res.size(), 2);
        TEST_ASSERT_EQ(res[0], 1);
        TEST_ASSERT_EQ(res[1], 2);
    }

    // 2. Field-Restricted Filename Search: "name:report" -> matches ONLY Doc 1
    {
        ResultSet res = evalQuery("name:report");
        TEST_ASSERT_EQ(res.size(), 1);
        TEST_ASSERT_EQ(res[0], 1);
    }

    // 3. Field-Restricted Body Search: "body:report" -> matches ONLY Doc 2
    {
        ResultSet res = evalQuery("body:report");
        TEST_ASSERT_EQ(res.size(), 1);
        TEST_ASSERT_EQ(res[0], 2);
    }

    // 4. Combined Field AND: "name:report AND body:statements" -> matches Doc 1
    {
        ResultSet res = evalQuery("name:report AND body:statements");
        TEST_ASSERT_EQ(res.size(), 1);
        TEST_ASSERT_EQ(res[0], 1);

        ResultSet resFail = evalQuery("name:budget AND body:statements");
        TEST_ASSERT(resFail.empty());
    }

    // 5. Combined Field OR: "name:invoice OR body:revenue" -> matches Doc 2 and Doc 3
    {
        ResultSet res = evalQuery("name:invoice OR body:revenue");
        TEST_ASSERT_EQ(res.size(), 2);
        TEST_ASSERT_EQ(res[0], 2);
        TEST_ASSERT_EQ(res[1], 3);
    }

    // 6. Field-Restricted NOT: "NOT name:temporary" -> matches Doc 1, 2, 3
    {
        ResultSet res = evalQuery("NOT name:temporary");
        TEST_ASSERT_EQ(res.size(), 3);
        TEST_ASSERT_EQ(res[0], 1);
        TEST_ASSERT_EQ(res[1], 2);
        TEST_ASSERT_EQ(res[2], 3);
    }

    // 7. Same Term in Both Fields: Doc 4 has "temporary" in filename and body
    {
        ResultSet resName = evalQuery("name:temporary");
        TEST_ASSERT_EQ(resName.size(), 1);
        TEST_ASSERT_EQ(resName[0], 4);

        ResultSet resBody = evalQuery("body:temporary");
        TEST_ASSERT_EQ(resBody.size(), 1);
        TEST_ASSERT_EQ(resBody[0], 4);

        ResultSet resBoth = evalQuery("name:temporary AND body:temporary");
        TEST_ASSERT_EQ(resBoth.size(), 1);
        TEST_ASSERT_EQ(resBoth[0], 4);
    }

    // 8. Missing Field Qualifier Value: "name:"
    {
        LexResult lex = lexer.tokenize("name:");
        TEST_ASSERT(!lex.success);
        TEST_ASSERT(lex.errorMessage.find("Missing field qualifier value") != std::string::npos);
    }

    // 9. Invalid Field Qualifier: "unknown:term"
    {
        LexResult lex = lexer.tokenize("unknown:term");
        TEST_ASSERT(!lex.success);
        TEST_ASSERT(lex.errorMessage.find("Unsupported or invalid field qualifier") != std::string::npos);
    }
}

} // namespace sse::test
