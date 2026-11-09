#include "TestHarness.hpp"
#include "indexing/TextNormalizer.hpp"
#include "indexing/TokenizerConfig.hpp"

namespace sse::test {

void runTextNormalizerTests() {
    std::cout << "[RUNNING] TextNormalizer Tests..." << std::endl;

    using namespace sse::indexing;

    // Test 1: ASCII Case Folding
    {
        TEST_ASSERT_EQ(TextNormalizer::toLowerAscii("Hello WORLD!"), std::string("hello world!"));
        TEST_ASSERT_EQ(TextNormalizer::toLowerAscii("C++ PROGRAMMING 123"), std::string("c++ programming 123"));
        TEST_ASSERT_EQ(TextNormalizer::toLowerAscii("already lowercase"), std::string("already lowercase"));
        TEST_ASSERT_EQ(TextNormalizer::toLowerAscii(""), std::string(""));
    }

    // Test 2: Character classification
    {
        TEST_ASSERT(TextNormalizer::isAsciiAlphanumeric('a'));
        TEST_ASSERT(TextNormalizer::isAsciiAlphanumeric('Z'));
        TEST_ASSERT(TextNormalizer::isAsciiAlphanumeric('9'));
        TEST_ASSERT(!TextNormalizer::isAsciiAlphanumeric(' '));
        TEST_ASSERT(!TextNormalizer::isAsciiAlphanumeric('!'));

        TEST_ASSERT(TextNormalizer::isAsciiPunctuation(','));
        TEST_ASSERT(TextNormalizer::isAsciiPunctuation('.'));
        TEST_ASSERT(TextNormalizer::isAsciiPunctuation('!'));
        TEST_ASSERT(TextNormalizer::isAsciiPunctuation('?'));
        TEST_ASSERT(TextNormalizer::isAsciiPunctuation(';'));
        TEST_ASSERT(TextNormalizer::isAsciiPunctuation(':'));
        TEST_ASSERT(TextNormalizer::isAsciiPunctuation('-'));
        TEST_ASSERT(TextNormalizer::isAsciiPunctuation('_'));
        TEST_ASSERT(!TextNormalizer::isAsciiPunctuation('a'));
        TEST_ASSERT(!TextNormalizer::isAsciiPunctuation('0'));

        TEST_ASSERT(TextNormalizer::isDelimiter(' '));
        TEST_ASSERT(TextNormalizer::isDelimiter('\t'));
        TEST_ASSERT(TextNormalizer::isDelimiter('\n'));
        TEST_ASSERT(TextNormalizer::isDelimiter('\r'));
        TEST_ASSERT(TextNormalizer::isDelimiter(','));
        TEST_ASSERT(TextNormalizer::isDelimiter('/'));
        TEST_ASSERT(!TextNormalizer::isDelimiter('x'));
        TEST_ASSERT(!TextNormalizer::isDelimiter('7'));
    }

    // Test 3: Standard RFC 3629 UTF-8 Validation (Valid Sequences)
    {
        // 1-byte ASCII
        TEST_ASSERT(TextNormalizer::isValidUtf8("Pure ASCII string 12345!"));
        // 2-byte Latin (Café -> \xC3\xA9)
        TEST_ASSERT(TextNormalizer::isValidUtf8("Caf\xC3\xA9"));
        // 3-byte Cyrillic, Arabic, Urdu
        // Urdu text "اردو" -> \xD8\xA7\xD8\xB1\xD8\xAF\xD9\x88
        TEST_ASSERT(TextNormalizer::isValidUtf8("\xD8\xA7\xD8\xB1\xD8\xAF\xD9\x88"));
        // 4-byte Supplementary plane (e.g. U+1F600 Emoji: \xF0\x9F\x98\x80)
        TEST_ASSERT(TextNormalizer::isValidUtf8("\xF0\x9F\x98\x80"));
        // Empty string
        TEST_ASSERT(TextNormalizer::isValidUtf8(""));
    }

    // Test 4: RFC 3629 UTF-8 Validation (Invalid, Overlong, and Surrogate sequences)
    {
        // Overlong 2-byte encoding of ASCII space (\xC0\xA0)
        const char overlong2[] = {'\xC0', '\xA0', '\0'};
        TEST_ASSERT(!TextNormalizer::isValidUtf8(overlong2));

        // Overlong 3-byte encoding (\xE0\x80\xAF)
        const char overlong3[] = {'\xE0', '\x80', '\xAF', '\0'};
        TEST_ASSERT(!TextNormalizer::isValidUtf8(overlong3));

        // Overlong 4-byte encoding (\xF0\x80\x80\xAF)
        const char overlong4[] = {'\xF0', '\x80', '\x80', '\xAF', '\0'};
        TEST_ASSERT(!TextNormalizer::isValidUtf8(overlong4));

        // UTF-16 surrogate half (U+D800: \xED\xA0\x80) - Strictly forbidden in UTF-8
        const char surrogate[] = {'\xED', '\xA0', '\x80', '\0'};
        TEST_ASSERT(!TextNormalizer::isValidUtf8(surrogate));

        // Out of Unicode range (> U+10FFFF: \xF4\x90\x80\x80)
        const char outOfRange[] = {'\xF4', '\x90', '\x80', '\x80', '\0'};
        TEST_ASSERT(!TextNormalizer::isValidUtf8(outOfRange));

        // Truncated multi-byte (lead byte without continuation)
        const char truncated[] = {'\xD8', '\0'};
        TEST_ASSERT(!TextNormalizer::isValidUtf8(truncated));

        // Invalid lead byte 0xFF
        const char invalidLead[] = {'\xFF', 'a', '\0'};
        TEST_ASSERT(!TextNormalizer::isValidUtf8(invalidLead));
    }

    // Test 5: Token Normalization with Config Rules
    {
        TokenizerConfig cfg;
        cfg.minTokenLength = 2;
        cfg.maxTokenLength = 10;
        cfg.caseFold = true;
        cfg.filterStopWords = true;
        cfg.addStopWord("the");
        cfg.addStopWord("and");

        // Normal token
        TEST_ASSERT_EQ(TextNormalizer::normalizeToken("Search", cfg), std::string("search"));
        // Token below min length
        TEST_ASSERT_EQ(TextNormalizer::normalizeToken("a", cfg), std::string(""));
        // Token exceeding max length
        TEST_ASSERT_EQ(TextNormalizer::normalizeToken("supercalifragilistic", cfg), std::string(""));
        // Stop word
        TEST_ASSERT_EQ(TextNormalizer::normalizeToken("The", cfg), std::string(""));
        TEST_ASSERT_EQ(TextNormalizer::normalizeToken("AND", cfg), std::string(""));
        // Valid multi-byte token
        TEST_ASSERT_EQ(TextNormalizer::normalizeToken("Caf\xC3\xA9", cfg), std::string("caf\xC3\xA9"));
    }
}

} // namespace sse::test
