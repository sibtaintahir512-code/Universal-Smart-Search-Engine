#include "TestHarness.hpp"
#include "indexing/FileScanner.hpp"
#include "indexing/FileMetadataManager.hpp"
#include "indexing/DocumentCatalog.hpp"
#include "indexing/TextNormalizer.hpp"
#include "indexing/Tokenizer.hpp"
#include "indexing/InvertedIndex.hpp"
#include "indexing/IndexingPipeline.hpp"
#include "extraction/ExtractionManager.hpp"
#include "search/QueryLexer.hpp"
#include "search/QueryParser.hpp"
#include "search/QueryEvaluator.hpp"
#include "search/SearchService.hpp"
#include "benchmarks/SyntheticCorpusGenerator.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace sse::test {

// =============================================================================
// IT-01: FileScanner + FileMetadataManager
// =============================================================================
void runIT01Tests() {
    std::cout << "\n[RUNNING IT-01] FileScanner + FileMetadataManager Integration Test..." << std::endl;

    std::filesystem::path testDir = "temp_it01_corpus";
    std::filesystem::remove_all(testDir);
    std::filesystem::create_directories(testDir / "sub1");
    std::filesystem::create_directories(testDir / "sub2");

    // Create 11 different file extensions across folders
    std::vector<std::pair<std::string, FileCategory>> fixtures = {
        {"testDir/sub1/doc.TXT", FileCategory::Documents},
        {"testDir/sub1/doc2.pdf", FileCategory::Documents},
        {"testDir/sub1/code.CPP", FileCategory::SourceCode},
        {"testDir/sub1/script.Py", FileCategory::SourceCode},
        {"testDir/sub2/photo.jpg", FileCategory::Images},
        {"testDir/sub2/graphic.PNG", FileCategory::Images},
        {"testDir/sub2/video.mp4", FileCategory::Videos},
        {"testDir/sub2/audio.mp3", FileCategory::Audio},
        {"testDir/sub2/bundle.zip", FileCategory::Archives},
        {"testDir/sub2/program.bin", FileCategory::Other},
        {"testDir/sub2/unknown.customext", FileCategory::Other}
    };

    for (const auto& f : fixtures) {
        std::string actualPath = f.first;
        // replace testDir prefix
        actualPath.replace(0, 7, testDir.string());
        std::ofstream out(actualPath, std::ios::binary);
        out << "fixture content for " << actualPath;
        out.close();
    }

    indexing::DocumentCatalog catalog;
    indexing::FileScanner scanner;
    scanner.addRoot(testDir.string());

    std::atomic<bool> cancelToken{false};
    uint64_t fileCount = 0;

    auto onFile = [&](const indexing::ExtendedFileRecord& record, FileChangeStatus status) {
        fileCount++;
        TEST_ASSERT(status == FileChangeStatus::New);
        TEST_ASSERT(!record.filename.empty());
        TEST_ASSERT(!record.canonicalPath.empty());
        // Verify case-insensitive extension classification
        FileCategory expectedCat = indexing::FileMetadataManager::classifyExtension(record.extension);
        TEST_ASSERT_EQ(static_cast<int>(record.metadata.category), static_cast<int>(expectedCat));
    };

    auto stats = scanner.scan(catalog, onFile, nullptr, cancelToken);

    TEST_ASSERT_EQ(stats.filesDiscovered, fixtures.size());
    TEST_ASSERT_EQ(catalog.activeCount(), fixtures.size());
    TEST_ASSERT_EQ(fileCount, fixtures.size());

    std::filesystem::remove_all(testDir);
}

// =============================================================================
// IT-02: ContentExtractor + TextProcessor
// =============================================================================
void runIT02Tests() {
    std::cout << "\n[RUNNING IT-02] ContentExtractor + TextProcessor Integration Test..." << std::endl;

    std::filesystem::path testDir = "temp_it02_corpus";
    std::filesystem::remove_all(testDir);
    std::filesystem::create_directories(testDir);

    auto filePath = testDir / "sample_text.txt";
    {
        std::ofstream out(filePath, std::ios::binary);
        // Uppercase, lowercase, punctuation, English stop words, underscores, hyphens, CamelCase, UTF-8
        out << "The Quick Brown Fox jumps over the Lazy Dog.\n";
        out << "Data_Structures and-Algorithms: dijkstra_algorithm, CamelCaseToken.\n";
        out << "Stopwords: an, and, the, of, in, with.\n";
        out << "UTF-8 content: café résumé naïve.\n";
        out.close();
    }

    extraction::ExtractionManager extractor;
    auto extResult = extractor.extract(filePath.string(), FileCategory::Documents);
    TEST_ASSERT(extResult.isSuccess());
    TEST_ASSERT(!extResult.extractedText.empty());

    indexing::Tokenizer tokenizer;
    auto tokens = tokenizer.tokenize(extResult.extractedText);

    TEST_ASSERT(tokens.size() > 0);

    // Verify normalization: lowercase
    bool foundQuick = false;
    bool foundDijkstra = false;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].term == "quick") foundQuick = true;
        if (tokens[i].term == "dijkstra") foundDijkstra = true;
    }

    TEST_ASSERT(foundQuick);
    TEST_ASSERT(foundDijkstra);

    std::filesystem::remove_all(testDir);
}

// =============================================================================
// IT-03: TextProcessor + InvertedIndex
// =============================================================================
void runIT03Tests() {
    std::cout << "\n[RUNNING IT-03] TextProcessor + InvertedIndex Integration Test..." << std::endl;

    indexing::DocumentCatalog catalog;
    indexing::InvertedIndex index;
    indexing::Tokenizer tokenizer;
    indexing::IndexingPipeline pipeline(catalog, index, tokenizer, nullptr);

    FileMetadata meta1;
    meta1.category = FileCategory::SourceCode;
    meta1.flags = META_FLAG_ACTIVE;
    DocID id1 = catalog.registerDocument(meta1, "/src/dijkstra.cpp", "dijkstra.cpp");

    FileMetadata meta2;
    meta2.category = FileCategory::SourceCode;
    meta2.flags = META_FLAG_ACTIVE;
    DocID id2 = catalog.registerDocument(meta2, "/src/astar.cpp", "astar.cpp");

    extraction::ExtractionResult res1;
    res1.docId = id1;
    res1.status = extraction::ExtractionStatus::Success;
    res1.extractedText = "dijkstra algorithm shortest path graph priority queue";

    extraction::ExtractionResult res2;
    res2.docId = id2;
    res2.status = extraction::ExtractionStatus::Success;
    res2.extractedText = "astar search algorithm heuristic shortest path graph";

    auto r1 = pipeline.indexDocument(id1, &res1);
    auto r2 = pipeline.indexDocument(id2, &res2);

    TEST_ASSERT(r1.success);
    TEST_ASSERT(r2.success);

    // Inspect Inverted Index
    TEST_ASSERT(index.containsTerm("algorithm"));
    TEST_ASSERT(index.containsTerm("dijkstra"));
    TEST_ASSERT(index.containsTerm("astar"));

    auto* plistAlg = index.getPostingList("algorithm");
    TEST_ASSERT(plistAlg != nullptr);
    TEST_ASSERT_EQ(plistAlg->size(), 2);

    // Verify ascending DocID order
    TEST_ASSERT_EQ(plistAlg->head()->docId, id1);
    TEST_ASSERT_EQ(plistAlg->head()->next->docId, id2);

    // Verify field separation (filename vs body)
    auto* plistDijkstra = index.getPostingList("dijkstra");
    TEST_ASSERT(plistDijkstra != nullptr);
    TEST_ASSERT(plistDijkstra->head()->nameTermFreq > 0);
    TEST_ASSERT(plistDijkstra->head()->bodyTermFreq > 0);
}

// =============================================================================
// IT-04: QueryParser + BooleanSearchEngine
// =============================================================================
void runIT04Tests() {
    std::cout << "\n[RUNNING IT-04] QueryParser + BooleanSearchEngine Integration Test..." << std::endl;

    indexing::DocumentCatalog catalog;
    indexing::InvertedIndex index;
    indexing::Tokenizer tokenizer;
    indexing::IndexingPipeline pipeline(catalog, index, tokenizer, nullptr);

    // Controlled corpus:
    // doc 1: cpp guide tutorial
    // doc 2: python guide tutorial
    // doc 3: cpp advanced manual
    // doc 4: rust guide book
    struct DocSpec {
        std::string path;
        std::string filename;
        std::string content;
    };
    std::vector<DocSpec> docs = {
        {"/docs/cpp_guide.txt", "cpp_guide.txt", "cpp programming guide for beginners"},
        {"/docs/py_guide.txt", "py_guide.txt", "python programming guide and tutorials"},
        {"/docs/cpp_adv.txt", "cpp_adv.txt", "cpp advanced systems programming manual"},
        {"/docs/rust_guide.txt", "rust_guide.txt", "rust programming guide"}
    };

    for (const auto& d : docs) {
        FileMetadata meta;
        meta.category = FileCategory::Documents;
        meta.flags = META_FLAG_ACTIVE;
        DocID id = catalog.registerDocument(meta, d.path, d.filename);
        extraction::ExtractionResult res;
        res.docId = id;
        res.status = extraction::ExtractionStatus::Success;
        res.extractedText = d.content;
        pipeline.indexDocument(id, &res);
    }

    search::SearchService service(&index, &catalog);

    // Required query: (cpp OR python) AND guide
    // Expected: doc 1 (cpp_guide) and doc 2 (py_guide)
    auto resp = service.search("(cpp OR python) AND guide");
    TEST_ASSERT_EQ(static_cast<int>(resp.status), static_cast<int>(search::SearchStatus::Success));
    TEST_ASSERT_EQ(resp.totalMatches, 2);

    // NOT query: guide AND NOT rust
    // Expected: doc 1 (cpp) and doc 2 (python)
    auto resp2 = service.search("guide AND NOT rust");
    TEST_ASSERT_EQ(resp2.totalMatches, 2);

    // Implicit AND: cpp guide
    // Expected: doc 1 only
    auto resp3 = service.search("cpp guide");
    TEST_ASSERT_EQ(resp3.totalMatches, 1);

    // Double negation / complex nesting: ((cpp OR python) AND NOT rust) AND guide
    auto resp4 = service.search("((cpp OR python) AND NOT rust) AND guide");
    TEST_ASSERT_EQ(resp4.totalMatches, 2);
}

} // namespace sse::test
