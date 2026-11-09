#include "TestHarness.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include "search/SearchService.hpp"
#include "search/QueryLexer.hpp"
#include "search/QueryParser.hpp"
#include "search/QueryEvaluator.hpp"

namespace sse::test {

static void buildBenchmarkCorpus(indexing::DocumentCatalog& catalog,
                                indexing::InvertedIndex& index,
                                std::size_t docCount) {
    for (std::size_t i = 1; i <= docCount; ++i) {
        DocID docId = static_cast<DocID>(i);
        FileMetadata meta;
        meta.category = FileCategory::Documents;
        meta.fileSize = 1024 + (i % 5000);
        meta.lastModifiedTime = 1700000000 + i;
        meta.extractionState = ExtractionStatus::Extracted;

        std::string fname = "file_" + std::to_string(i) + ".txt";
        std::string fpath = "/corpus/" + fname;
        catalog.registerDocument(meta, fpath, fname);

        // Universal term: every document has "common"
        index.addTerm("common", docId, 0, 1, FIELD_CONTENT);

        // Halving term: every even doc has "even"
        if (i % 2 == 0) {
            index.addTerm("even", docId, 0, 1, FIELD_CONTENT);
        }

        // Quarter term: every doc divisible by 4 has "quarter"
        if (i % 4 == 0) {
            index.addTerm("quarter", docId, 0, 1, FIELD_CONTENT);
        }

        // Tenth term: every doc divisible by 10 has "tenth"
        if (i % 10 == 0) {
            index.addTerm("tenth", docId, 0, 1, FIELD_CONTENT);
        }

        // Unique term per document
        std::string uniqueTerm = "term" + std::to_string(i);
        index.addTerm(uniqueTerm, docId, 0, 1, FIELD_CONTENT);

        // Filename term
        index.addTerm("docfile", docId, 1, 0, FIELD_FILENAME);
    }
}

void runPhase6BenchmarkTests() {
    std::cout << "\n[BENCHMARK] Executing Phase 6 Search Engine Performance Benchmarks..." << std::endl;

    using namespace sse::indexing;
    using namespace sse::search;

    const std::size_t scales[] = {100, 1000, 10000};

    for (std::size_t scale : scales) {
        DocumentCatalog catalog;
        InvertedIndex index;

        buildBenchmarkCorpus(catalog, index, scale);
        SearchService service(&index, &catalog);

        const std::string queries[] = {
            "common",                                  // Universal single term
            "term50",                                  // Rare single term
            "nonexistent",                             // Zero hits
            "even AND quarter",                        // Conjunction
            "even OR quarter",                         // Disjunction
            "common AND NOT even",                     // Negation / difference
            "(even OR quarter) AND tenth",             // Nested parentheses
            "name:docfile AND (even OR quarter)",      // Field-specific search
            "common AND NOT (even OR quarter)"         // Complex De Morgan
        };

        std::cout << "  --------------------------------------------------------" << std::endl;
        std::cout << "  Corpus Scale          : " << scale << " documents" << std::endl;
        std::cout << "  Vocabulary Size       : " << index.vocabularySize() << " terms" << std::endl;
        std::cout << "  Total Postings Count  : " << index.totalPostingsCount() << " postings" << std::endl;

        for (const auto& q : queries) {
            // Measure individual stages: lex, parse, eval
            QueryLexer lexer;
            auto t0 = std::chrono::high_resolution_clock::now();
            LexResult lex = lexer.tokenize(q);
            auto t1 = std::chrono::high_resolution_clock::now();

            QueryParser parser;
            ParseResult parse = parser.parse(lex.tokens);
            auto t2 = std::chrono::high_resolution_clock::now();

            QueryEvaluator eval(&index, &catalog);
            EvaluationResult evalRes = eval.evaluate(parse.root.get());
            auto t3 = std::chrono::high_resolution_clock::now();

            // End-to-end via service
            auto s0 = std::chrono::high_resolution_clock::now();
            SearchResponse resp = service.search(q, 0, 50);
            auto s1 = std::chrono::high_resolution_clock::now();

            TEST_ASSERT(resp.isValid);
            TEST_ASSERT_EQ(resp.status, SearchStatus::Success);
            TEST_ASSERT(resp.totalMatches <= scale);

            double lexUs = std::chrono::duration<double, std::micro>(t1 - t0).count();
            double parseUs = std::chrono::duration<double, std::micro>(t2 - t1).count();
            double evalUs = std::chrono::duration<double, std::micro>(t3 - t2).count();
            double totalUs = std::chrono::duration<double, std::micro>(s1 - s0).count();

            std::cout << "  Query: \"" << std::left << std::setw(36) << q << "\""
                      << " Matches: " << std::right << std::setw(5) << resp.totalMatches
                      << " | Parse: " << std::fixed << std::setprecision(1) << std::setw(5) << (lexUs + parseUs) << " µs"
                      << " | Eval: " << std::setw(6) << evalUs << " µs"
                      << " | Total: " << std::setw(6) << totalUs << " µs"
                      << std::endl;
        }
        std::cout << "  --------------------------------------------------------" << std::endl;
    }
}

} // namespace sse::test
