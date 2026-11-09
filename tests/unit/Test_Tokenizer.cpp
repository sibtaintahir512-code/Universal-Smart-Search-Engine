#include "TestHarness.hpp"
#include "indexing/Tokenizer.hpp"

namespace sse::test {

void runTokenizerTests() {
    std::cout << "[RUNNING] Tokenizer Tests..." << std::endl;

    using namespace sse::indexing;

    // Test 1: Empty and Whitespace-Only Input
    {
        Tokenizer tokenizer;
        auto tokensEmpty = tokenizer.tokenize("");
        TEST_ASSERT(tokensEmpty.empty());

        auto tokensWhitespace = tokenizer.tokenize("   \t\n\r   \n\t  ");
        TEST_ASSERT(tokensWhitespace.empty());
    }

    // Test 2: Basic Words and Case Folding
    {
        Tokenizer tokenizer;
        auto tokens = tokenizer.tokenize("Hello WORLD");
        TEST_ASSERT_EQ(tokens.size(), 2ULL);
        TEST_ASSERT_EQ(tokens[0].term, std::string("hello"));
        TEST_ASSERT_EQ(tokens[0].position, 0U);
        TEST_ASSERT_EQ(tokens[1].term, std::string("world"));
        TEST_ASSERT_EQ(tokens[1].position, 1U);
    }

    // Test 3: Punctuation and Numbers (Specification Example: Search, Engine! Version 2.0)
    {
        Tokenizer tokenizer;
        auto tokens = tokenizer.tokenize("Search, Engine! Version 2.0");
        TEST_ASSERT_EQ(tokens.size(), 5ULL);
        TEST_ASSERT_EQ(tokens[0].term, std::string("search"));
        TEST_ASSERT_EQ(tokens[0].position, 0U);
        TEST_ASSERT_EQ(tokens[1].term, std::string("engine"));
        TEST_ASSERT_EQ(tokens[1].position, 1U);
        TEST_ASSERT_EQ(tokens[2].term, std::string("version"));
        TEST_ASSERT_EQ(tokens[2].position, 2U);
        TEST_ASSERT_EQ(tokens[3].term, std::string("2"));
        TEST_ASSERT_EQ(tokens[3].position, 3U);
        TEST_ASSERT_EQ(tokens[4].term, std::string("0"));
        TEST_ASSERT_EQ(tokens[4].position, 4U);
    }

    // Test 4: Mixed Alphanumeric Tokens and C++ / C# (Specification Example)
    {
        Tokenizer tokenizer;
        auto tokens = tokenizer.tokenize("C++ Programming and C# Development");
        TEST_ASSERT_EQ(tokens.size(), 5ULL);
        TEST_ASSERT_EQ(tokens[0].term, std::string("c"));
        TEST_ASSERT_EQ(tokens[0].position, 0U);
        TEST_ASSERT_EQ(tokens[1].term, std::string("programming"));
        TEST_ASSERT_EQ(tokens[1].position, 1U);
        TEST_ASSERT_EQ(tokens[2].term, std::string("and"));
        TEST_ASSERT_EQ(tokens[2].position, 2U);
        TEST_ASSERT_EQ(tokens[3].term, std::string("c"));
        TEST_ASSERT_EQ(tokens[3].position, 3U);
        TEST_ASSERT_EQ(tokens[4].term, std::string("development"));
        TEST_ASSERT_EQ(tokens[4].position, 4U);
    }

    // Test 5: Repeated Separators and Punctuation-Only Strings
    {
        Tokenizer tokenizer;
        auto tokensPunct = tokenizer.tokenize("!!! ??? ,,, ... ;;; ---");
        TEST_ASSERT(tokensPunct.empty());

        auto tokensRepeated = tokenizer.tokenize("data...structures---algorithms");
        TEST_ASSERT_EQ(tokensRepeated.size(), 3ULL);
        TEST_ASSERT_EQ(tokensRepeated[0].term, std::string("data"));
        TEST_ASSERT_EQ(tokensRepeated[1].term, std::string("structures"));
        TEST_ASSERT_EQ(tokensRepeated[2].term, std::string("algorithms"));
    }

    // Test 6: Deterministic Monotonic Position Tracking
    {
        Tokenizer tokenizer;
        auto tokens = tokenizer.tokenize("the quick brown fox");
        TEST_ASSERT_EQ(tokens.size(), 4ULL);
        TEST_ASSERT_EQ(tokens[0].term, std::string("the"));
        TEST_ASSERT_EQ(tokens[0].position, 0U);
        TEST_ASSERT_EQ(tokens[1].term, std::string("quick"));
        TEST_ASSERT_EQ(tokens[1].position, 1U);
        TEST_ASSERT_EQ(tokens[2].term, std::string("brown"));
        TEST_ASSERT_EQ(tokens[2].position, 2U);
        TEST_ASSERT_EQ(tokens[3].term, std::string("fox"));
        TEST_ASSERT_EQ(tokens[3].position, 3U);
    }

    // Test 7: Stop-Word Filtering and Positional Gap Preservation
    {
        TokenizerConfig cfg;
        cfg.filterStopWords = true;
        cfg.addStopWord("the");
        cfg.addStopWord("and");

        Tokenizer tokenizer(cfg);
        auto tokens = tokenizer.tokenize("the quick and the brown fox");
        // "the" (pos 0) skipped
        // "quick" (pos 1) kept
        // "and" (pos 2) skipped
        // "the" (pos 3) skipped
        // "brown" (pos 4) kept
        // "fox" (pos 5) kept
        TEST_ASSERT_EQ(tokens.size(), 3ULL);
        TEST_ASSERT_EQ(tokens[0].term, std::string("quick"));
        TEST_ASSERT_EQ(tokens[0].position, 1U); // Preserved original position
        TEST_ASSERT_EQ(tokens[1].term, std::string("brown"));
        TEST_ASSERT_EQ(tokens[1].position, 4U); // Preserved original position
        TEST_ASSERT_EQ(tokens[2].term, std::string("fox"));
        TEST_ASSERT_EQ(tokens[2].position, 5U); // Preserved original position
    }

    // Test 8: Min and Max Token Length Bounding
    {
        TokenizerConfig cfg;
        cfg.minTokenLength = 3;
        cfg.maxTokenLength = 6;

        Tokenizer tokenizer(cfg);
        auto tokens = tokenizer.tokenize("a in cat elephant tiny supercalifragilistic");
        // "a" (len 1) skipped
        // "in" (len 2) skipped
        // "cat" (len 3) kept
        // "elephant" (len 8) skipped
        // "tiny" (len 4) kept
        // "supercalifragilistic" (len 20) skipped
        TEST_ASSERT_EQ(tokens.size(), 2ULL);
        TEST_ASSERT_EQ(tokens[0].term, std::string("cat"));
        TEST_ASSERT_EQ(tokens[1].term, std::string("tiny"));
    }

    // Test 9: Valid Multi-Byte UTF-8 Tokens
    {
        Tokenizer tokenizer;
        // Text containing accented Latin, Urdu, and English
        std::string mixedText = "Caf\xC3\xA9 r\xC3\xA9sum\xC3\xA9 \xD8\xA7\xD8\xB1\xD8\xAF\xD9\x88 search";
        auto tokens = tokenizer.tokenize(mixedText);
        TEST_ASSERT_EQ(tokens.size(), 4ULL);
        TEST_ASSERT_EQ(tokens[0].term, std::string("caf\xC3\xA9"));
        TEST_ASSERT_EQ(tokens[1].term, std::string("r\xC3\xA9sum\xC3\xA9"));
        TEST_ASSERT_EQ(tokens[2].term, std::string("\xD8\xA7\xD8\xB1\xD8\xAF\xD9\x88"));
        TEST_ASSERT_EQ(tokens[3].term, std::string("search"));
    }

    // Test 10: Invalid UTF-8 Graceful Handling (No Crashing)
    {
        Tokenizer tokenizer;
        const char badBytes[] = {'g', 'o', 'o', 'd', ' ', (char)0xFF, (char)0xFE, ' ', 't', 'e', 'x', 't', '\0'};
        auto tokens = tokenizer.tokenize(std::string_view(badBytes, 12));
        TEST_ASSERT(tokens.size() >= 2ULL);
        TEST_ASSERT_EQ(tokens[0].term, std::string("good"));
        TEST_ASSERT_EQ(tokens[tokens.size() - 1].term, std::string("text"));
    }

    // Test 11: Idempotence & Determinism
    {
        Tokenizer tokenizer;
        std::string sample = "Universal Smart Search Engine Phase 5 Verification";
        auto t1 = tokenizer.tokenize(sample);
        auto t2 = tokenizer.tokenize(sample);
        TEST_ASSERT_EQ(t1.size(), t2.size());
        for (std::size_t i = 0; i < t1.size(); ++i) {
            TEST_ASSERT_EQ(t1[i].term, t2[i].term);
            TEST_ASSERT_EQ(t1[i].position, t2[i].position);
            TEST_ASSERT_EQ(t1[i].charOffset, t2[i].charOffset);
        }
    }
}

} // namespace sse::test
