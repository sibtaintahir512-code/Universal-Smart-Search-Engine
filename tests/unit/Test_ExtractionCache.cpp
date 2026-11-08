#include "TestHarness.hpp"
#include "extraction/ExtractionCache.hpp"

namespace sse::test {

void runExtractionCacheTests() {
    std::cout << "[RUNNING] ExtractionCache Tests..." << std::endl;

    using namespace sse::extraction;
    using sse::extraction::ExtractionStatus;

    ExtractionCache cache;

    // Test 1: Empty cache lookup
    {
        ExtractionResult out;
        bool hit = cache.get("/docs/test.txt", 100, 1000, "TextExtractor", "1.0.0", out);
        TEST_ASSERT(!hit);
        TEST_ASSERT_EQ(cache.size(), 0);
    }

    // Test 2: Cache Hit
    {
        ExtractionResult res;
        res.sourcePath = "/docs/test.txt";
        res.originalFileSize = 500;
        res.extractedText = "Cached Text Content";
        res.status = ExtractionStatus::Success;
        res.extractorId = "TextExtractor";
        res.extractorVersion = "1.0.0";

        cache.put(res, 2000); // modTime = 2000

        TEST_ASSERT_EQ(cache.size(), 1);

        ExtractionResult cachedOut;
        bool hit = cache.get("/docs/test.txt", 500, 2000, "TextExtractor", "1.0.0", cachedOut);
        TEST_ASSERT(hit);
        TEST_ASSERT_EQ(cachedOut.extractedText, std::string("Cached Text Content"));
    }

    // Test 3: Invalidation on File Size change
    {
        ExtractionResult cachedOut;
        // Current size 600 != cached size 500
        bool hit = cache.get("/docs/test.txt", 600, 2000, "TextExtractor", "1.0.0", cachedOut);
        TEST_ASSERT(!hit); // Invalidated
    }

    // Test 4: Invalidation on Modification Timestamp change
    {
        ExtractionResult cachedOut;
        // Current modTime 2050 != cached modTime 2000
        bool hit = cache.get("/docs/test.txt", 500, 2050, "TextExtractor", "1.0.0", cachedOut);
        TEST_ASSERT(!hit); // Invalidated
    }

    // Test 5: Invalidation on Extractor Version change
    {
        ExtractionResult cachedOut;
        // Version "1.1.0" != cached "1.0.0"
        bool hit = cache.get("/docs/test.txt", 500, 2000, "TextExtractor", "1.1.0", cachedOut);
        TEST_ASSERT(!hit); // Invalidated
    }

    // Test 6: Invalidation on Extractor ID change
    {
        ExtractionResult cachedOut;
        bool hit = cache.get("/docs/test.txt", 500, 2000, "OtherExtractor", "1.0.0", cachedOut);
        TEST_ASSERT(!hit);
    }

    // Test 7: Explicit Invalidate & Clear
    {
        TEST_ASSERT(cache.invalidate("/docs/test.txt"));
        TEST_ASSERT_EQ(cache.size(), 0);

        ExtractionResult res;
        res.sourcePath = "/docs/another.txt";
        cache.put(res, 100);
        TEST_ASSERT_EQ(cache.size(), 1);

        cache.clear();
        TEST_ASSERT_EQ(cache.size(), 0);
    }
}

} // namespace sse::test
