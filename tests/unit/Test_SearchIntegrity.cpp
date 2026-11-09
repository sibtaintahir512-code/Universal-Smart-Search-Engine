#include "TestHarness.hpp"
#include <string>
#include "indexing/DocumentCatalog.hpp"
#include "indexing/InvertedIndex.hpp"
#include "search/SearchService.hpp"
#include "search/SearchIntegrityChecker.hpp"

namespace sse::test {

void runSearchIntegrityTests() {
    std::cout << "[RUNNING] SearchIntegrity Tests..." << std::endl;

    using namespace sse::indexing;
    using namespace sse::search;

    DocumentCatalog catalog;
    InvertedIndex index;

    // Register 20 test documents with distinct term distributions
    for (DocID id = 1; id <= 20; ++id) {
        FileMetadata meta;
        meta.category = FileCategory::Documents;
        meta.fileSize = 512 * id;
        meta.extractionState = ExtractionStatus::Extracted;

        std::string fname = "report_" + std::to_string(id) + ".pdf";
        catalog.registerDocument(meta, "/archive/" + fname, fname);

        // Every doc has "common"
        index.addTerm("common", id, 0, 1, FIELD_CONTENT);

        // Even docs have "even"
        if (id % 2 == 0) {
            index.addTerm("even", id, 0, 1, FIELD_CONTENT);
        }

        // Multiples of 5 have "fives"
        if (id % 5 == 0) {
            index.addTerm("fives", id, 0, 1, FIELD_CONTENT);
        }

        // Name terms
        index.addTerm("report", id, 1, 0, FIELD_FILENAME);
    }

    SearchService service(&index, &catalog);

    // 1. Audit Invariants via SearchIntegrityChecker
    {
        SearchIntegrityReport rep = SearchIntegrityChecker::verifyInvariants(service, index, catalog);
        TEST_ASSERT(rep.passed);
        TEST_ASSERT(rep.violations.empty());
        TEST_ASSERT_EQ(rep.checksRun, rep.checksPassed);
    }

    // 2. Invariant: Search Never Mutates Inverted Index
    {
        const std::size_t initialVocab = index.vocabularySize();
        const uint64_t initialPostings = index.totalPostingsCount();

        service.search("common AND even");
        service.search("even OR fives");
        service.search("NOT fives");
        service.search("name:report AND (even OR fives)");
        service.search("nonexistent AND something");

        TEST_ASSERT_EQ(index.vocabularySize(), initialVocab);
        TEST_ASSERT_EQ(index.totalPostingsCount(), initialPostings);
    }

    // 3. Invariant: Ascending DocID Order & Valid Active Documents
    {
        SearchResponse resp = service.search("even OR fives");
        TEST_ASSERT(resp.isValid);
        TEST_ASSERT(!resp.matchingDocIds.empty());

        for (std::size_t i = 0; i < resp.matchingDocIds.size(); ++i) {
            DocID id = resp.matchingDocIds[i];
            TEST_ASSERT(catalog.isActive(id));
            if (i > 0) {
                TEST_ASSERT(resp.matchingDocIds[i] > resp.matchingDocIds[i - 1]); // Strict ascending order
            }
        }
    }

    // 4. Invariant: Exact Set Intersection Verification
    {
        // "even AND fives" -> IDs divisible by 2 and 5 (i.e. 10 and 20)
        SearchResponse resp = service.search("even AND fives");
        TEST_ASSERT(resp.isValid);
        TEST_ASSERT_EQ(resp.totalMatches, 2);
        TEST_ASSERT_EQ(resp.matchingDocIds[0], 10);
        TEST_ASSERT_EQ(resp.matchingDocIds[1], 20);
    }

    // 5. Invariant: Exact Set Difference (Universe \ Exclude) Verification
    {
        // "common AND NOT even" -> odd numbered documents (1, 3, 5, 7, 9, 11, 13, 15, 17, 19)
        SearchResponse resp = service.search("common AND NOT even");
        TEST_ASSERT(resp.isValid);
        TEST_ASSERT_EQ(resp.totalMatches, 10);
        for (std::size_t i = 0; i < resp.totalMatches; ++i) {
            DocID expectedId = static_cast<DocID>(2 * i + 1);
            TEST_ASSERT_EQ(resp.matchingDocIds[i], expectedId);
        }
    }
}

} // namespace sse::test
