#include "TestHarness.hpp"
#include "dsa/CustomStack.hpp"
#include <string>

namespace sse::test {

void runStackTests() {
    std::cout << "[RUNNING] CustomStack Tests..." << std::endl;

    // Test 1: Empty state & underflow
    {
        CustomStack<int> stack;
        TEST_ASSERT(stack.empty());
        TEST_ASSERT_EQ(stack.size(), 0);
        TEST_ASSERT_THROWS(stack.top(), std::underflow_error);
        TEST_ASSERT_THROWS(stack.pop(), std::underflow_error);
    }

    // Test 2: LIFO Invariant
    {
        CustomStack<std::string> stack;
        stack.push("Alpha");
        stack.push("Beta");
        stack.push("Gamma");

        TEST_ASSERT_EQ(stack.size(), 3);
        TEST_ASSERT_EQ(stack.top(), std::string("Gamma"));

        stack.pop();
        TEST_ASSERT_EQ(stack.top(), std::string("Beta"));

        stack.pop();
        TEST_ASSERT_EQ(stack.top(), std::string("Alpha"));

        stack.pop();
        TEST_ASSERT(stack.empty());
    }

    // Test 3: Emplace & Move
    {
        CustomStack<std::string> s1;
        s1.emplace("ConstructedInPlace");
        TEST_ASSERT_EQ(s1.top(), std::string("ConstructedInPlace"));

        CustomStack<std::string> s2(std::move(s1));
        TEST_ASSERT_EQ(s2.size(), 1);
        TEST_ASSERT(s1.empty());
    }

    // Test 4: Query Parsing Parenthesis Matching Simulation
    {
        CustomStack<char> parenStack;
        std::string query = "(A AND (B OR C))";
        bool balanced = true;

        for (char c : query) {
            if (c == '(') {
                parenStack.push(c);
            } else if (c == ')') {
                if (parenStack.empty()) {
                    balanced = false;
                    break;
                }
                parenStack.pop();
            }
        }
        if (!parenStack.empty()) balanced = false;

        TEST_ASSERT(balanced);
    }
}

} // namespace sse::test
