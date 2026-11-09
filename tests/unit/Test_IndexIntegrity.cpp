#include "TestHarness.hpp"
#include "indexing/IndexIntegrityChecker.hpp"
#include "indexing/InvertedIndex.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/IndexingPipeline.hpp"

namespace sse::test {

void runIndexIntegrityTests() {
    std::cout << "[RUNNING] IndexIntegrityChecker Tests..." << std::endl;

    using namespace sse::indexing;
    using namespace sse::extraction;
    using sse::extraction::ExtractionStatus;

    // Test 1: Empty and Normal Valid Indexes Pass Verification
    {
        InvertedIndex index;
        IntegrityReport repEmpty = IndexIntegrityChecker::checkIntegrity(index);
        TEST_ASSERT(repEmpty.isValid);
        TEST_ASSERT_EQ(repEmpty.errorCount, 0ULL);

        // Add valid terms
        index.addTerm("compiler", 1, 1, 2, FIELD_FILENAME | FIELD_CONTENT);
        index.addTerm("compiler", 2, 0, 1, FIELD_CONTENT);
        index.addTerm("linker", 2, 1, 0, FIELD_FILENAME);

        IntegrityReport repValid = IndexIntegrityChecker::checkIntegrity(index);
        TEST_ASSERT(repValid.isValid);
        TEST_ASSERT_EQ(repValid.errorCount, 0ULL);
        TEST_ASSERT_EQ(repValid.totalTermsChecked, 2ULL);
        TEST_ASSERT_EQ(repValid.totalPostingsChecked, 3ULL);
    }

    // Test 2: Full Pipeline Indexing Passes Integrity Verification
    {
        DocumentCatalog catalog;
        InvertedIndex index;
        IndexingPipeline pipeline(catalog, index);

        FileMetadata m1; m1.fileSize = 100;
        DocID d1 = catalog.registerDocument(m1, "/src/main.cpp", "main.cpp");
        FileMetadata m2; m2.fileSize = 200;
        DocID d2 = catalog.registerDocument(m2, "/src/parser.cpp", "parser.cpp");

        ExtractionResult e1;
        e1.docId = d1;
        e1.status = ExtractionStatus::Success;
        e1.extractedText = "int main() { return parser_run(); }";
        pipeline.indexDocument(d1, &e1);

        ExtractionResult e2;
        e2.docId = d2;
        e2.status = ExtractionStatus::Success;
        e2.extractedText = "void parser_run() { init(); }";
        pipeline.indexDocument(d2, &e2);

        IntegrityReport rep = IndexIntegrityChecker::checkIntegrity(index, &catalog);
        TEST_ASSERT(rep.isValid);
        TEST_ASSERT_EQ(rep.errorCount, 0ULL);
    }

    // Test 3: Detecting Corrupted State — Zero Frequencies
    {
        InvertedIndex index;
        index.addTerm("faulty", 1, 0, 0, FIELD_NONE); // Zero frequencies!
        IntegrityReport rep = IndexIntegrityChecker::checkIntegrity(index);
        TEST_ASSERT(!rep.isValid);
        TEST_ASSERT(rep.errorCount > 0);
    }

    // Test 4: Detecting Corrupted State — Bitmask Inconsistency
    {
        InvertedIndex index;
        // nameTermFreq > 0 but mask is FIELD_CONTENT only!
        index.addTerm("inconsistent", 1, 3, 0, FIELD_CONTENT);
        IntegrityReport rep = IndexIntegrityChecker::checkIntegrity(index);
        TEST_ASSERT(!rep.isValid);
        TEST_ASSERT(rep.errorCount > 0);
    }

    // Test 5: Detecting Corrupted State — DocID Exceeding Catalog Total
    {
        DocumentCatalog catalog; // empty catalog (totalCount = 0)
        InvertedIndex index;
        index.addTerm("out_of_bounds", 50, 1, 0, FIELD_FILENAME);

        IntegrityReport rep = IndexIntegrityChecker::checkIntegrity(index, &catalog);
        TEST_ASSERT(!rep.isValid);
        TEST_ASSERT(rep.errorCount > 0);
    }

    // Test 6: Integrity Maintained After Update and Removal
    {
        DocumentCatalog catalog;
        InvertedIndex index;
        IndexingPipeline pipeline(catalog, index);

        FileMetadata m;
        DocID d1 = catalog.registerDocument(m, "/notes.txt", "notes.txt");
        DocID d2 = catalog.registerDocument(m, "/memo.txt", "memo.txt");

        ExtractionResult e1; e1.docId = d1; e1.status = ExtractionStatus::Success; e1.extractedText = "Meeting notes";
        ExtractionResult e2; e2.docId = d2; e2.status = ExtractionStatus::Success; e2.extractedText = "Urgent memo notes";
        pipeline.indexDocument(d1, &e1);
        pipeline.indexDocument(d2, &e2);

        // Update d1
        ExtractionResult e1Up; e1Up.docId = d1; e1Up.status = ExtractionStatus::Success; e1Up.extractedText = "Revised plan";
        pipeline.updateDocument(d1, &e1Up);

        IntegrityReport repAfterUpdate = IndexIntegrityChecker::checkIntegrity(index, &catalog);
        TEST_ASSERT(repAfterUpdate.isValid);

        // Remove d2
        pipeline.removeDocument(d2);
        IntegrityReport repAfterRemove = IndexIntegrityChecker::checkIntegrity(index, &catalog);
        TEST_ASSERT(repAfterRemove.isValid);
    }
}

} // namespace sse::test
