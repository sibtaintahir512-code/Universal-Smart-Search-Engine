#include "TestHarness.hpp"
#include "extraction/SourceCodeExtractor.hpp"
#include <fstream>
#include <filesystem>

namespace sse::test {

void runSourceCodeExtractorTests() {
    std::cout << "[RUNNING] SourceCodeExtractor Tests..." << std::endl;

    using namespace sse::extraction;
    using sse::extraction::ExtractionStatus;

    auto tempDir = std::filesystem::temp_directory_path() / "sse_code_extractor_test";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);

    SourceCodeExtractor extractor;

    // Test 1: Extension Support Checks
    {
        TEST_ASSERT(extractor.supportsExtension("cpp"));
        TEST_ASSERT(extractor.supportsExtension("py"));
        TEST_ASSERT(extractor.supportsExtension("java"));
        TEST_ASSERT(extractor.supportsExtension("js"));
        TEST_ASSERT(extractor.supportsExtension("html"));
        TEST_ASSERT(extractor.supportsExtension("sql"));
        TEST_ASSERT(!extractor.supportsExtension("pdf"));
        TEST_ASSERT(!extractor.supportsExtension("docx"));
        TEST_ASSERT(extractor.supportsCategory(FileCategory::SourceCode));
    }

    // Test 2: C++ File Extraction preserving line structure & indentation
    {
        auto fCpp = tempDir / "main.cpp";
        std::string sourceCode =
            "#include <iostream>\n"
            "\n"
            "int main() {\n"
            "    // Indented block\n"
            "    std::cout << \"Hello C++17!\" << std::endl;\n"
            "    return 0;\n"
            "}\n";

        { std::ofstream(fCpp) << sourceCode; }

        ExtractionResult res = extractor.extract(fCpp.u8string(), 1024 * 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Success);
        TEST_ASSERT_EQ(res.extractedText, sourceCode);
        TEST_ASSERT(!res.isTruncated);
    }

    // Test 3: Empty source code file
    {
        auto fEmpty = tempDir / "empty.py";
        { std::ofstream out(fEmpty); }

        ExtractionResult res = extractor.extract(fEmpty.u8string(), 1024);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT_EQ(res.extractedByteCount, 0ULL);
        TEST_ASSERT_EQ(res.extractedText, std::string(""));
    }

    // Test 4: Truncated source code extraction
    {
        auto fSql = tempDir / "schema.sql";
        std::string sql(500, '-');
        { std::ofstream(fSql) << sql; }

        ExtractionResult res = extractor.extract(fSql.u8string(), 50);
        TEST_ASSERT(res.isSuccess());
        TEST_ASSERT_EQ(res.status, ExtractionStatus::Partial);
        TEST_ASSERT(res.isTruncated);
        TEST_ASSERT_EQ(res.extractedByteCount, 50ULL);
    }

    std::filesystem::remove_all(tempDir);
}

} // namespace sse::test
