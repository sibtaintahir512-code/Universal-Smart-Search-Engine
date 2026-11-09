#pragma once

#include <iostream>
#include <string>
#include <exception>
#include <cmath>

namespace sse::dsa {
    // Forward declare namespace
}

namespace sse::test {

using namespace sse::dsa;

inline int g_totalTests = 0;
inline int g_passedTests = 0;
inline int g_failedTests = 0;

inline void reportTestResult(const char* testName, bool passed, const char* file, int line, const char* expr) {
    ++g_totalTests;
    if (passed) {
        ++g_passedTests;
    } else {
        ++g_failedTests;
        std::cerr << "  [FAILED] " << testName << " at " << file << ":" << line << "\n"
                  << "           Expression: " << expr << std::endl;
    }
}

} // namespace sse::test

#define TEST_ASSERT(expr) \
    do { \
        bool _res = static_cast<bool>(expr); \
        sse::test::reportTestResult(__func__, _res, __FILE__, __LINE__, #expr); \
    } while (0)

#define TEST_ASSERT_EQ(a, b) \
    do { \
        bool _res = ((a) == (b)); \
        sse::test::reportTestResult(__func__, _res, __FILE__, __LINE__, #a " == " #b); \
    } while (0)

#define TEST_ASSERT_NE(a, b) \
    do { \
        bool _res = ((a) != (b)); \
        sse::test::reportTestResult(__func__, _res, __FILE__, __LINE__, #a " != " #b); \
    } while (0)

#define TEST_ASSERT_NEAR(a, b, eps) \
    do { \
        double _diff = std::abs(static_cast<double>(a) - static_cast<double>(b)); \
        bool _res = (_diff <= static_cast<double>(eps)); \
        sse::test::reportTestResult(__func__, _res, __FILE__, __LINE__, "|" #a " - " #b "| <= " #eps); \
    } while (0)

#define TEST_ASSERT_THROWS(expr, ExceptionType) \
    do { \
        bool _threw = false; \
        try { \
            expr; \
        } catch (const ExceptionType&) { \
            _threw = true; \
        } catch (...) { \
            _threw = false; \
        } \
        sse::test::reportTestResult(__func__, _threw, __FILE__, __LINE__, #expr " throws " #ExceptionType); \
    } while (0)
