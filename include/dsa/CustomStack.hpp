#pragma once

#include <cstddef>
#include <stdexcept>
#include <utility>
#include "dsa/CustomDynamicArray.hpp"

namespace sse::dsa {

/**
 * @brief LIFO Stack backed by contiguous CustomDynamicArray<T>.
 * 
 * Used for:
 * 1. Shunting-Yard Boolean query parsing to RPN.
 * 2. RPN postfix posting list evaluation stack.
 * 
 * All operations run in O(1) amortized time with optimal cache locality.
 */
template <typename T>
class CustomStack {
public:
    using size_type = std::size_t;
    using value_type = T;

    CustomStack() = default;
    ~CustomStack() = default;

    // Rule of Five
    CustomStack(const CustomStack& other) = default;
    CustomStack(CustomStack&& other) noexcept = default;
    CustomStack& operator=(const CustomStack& other) = default;
    CustomStack& operator=(CustomStack&& other) noexcept = default;

    /**
     * @brief Pushes an item onto the top of the stack. O(1) amortized.
     */
    void push(const T& value) {
        m_data.push_back(value);
    }

    void push(T&& value) {
        m_data.push_back(std::move(value));
    }

    template <typename... Args>
    void emplace(Args&&... args) {
        m_data.emplace_back(std::forward<Args>(args)...);
    }

    /**
     * @brief Removes the item at the top of the stack. O(1).
     */
    void pop() {
        if (m_data.empty()) {
            throw std::underflow_error("CustomStack::pop() called on empty stack");
        }
        m_data.pop_back();
    }

    /**
     * @brief Returns a reference to the top item. O(1).
     */
    T& top() {
        if (m_data.empty()) {
            throw std::underflow_error("CustomStack::top() called on empty stack");
        }
        return m_data.back();
    }

    const T& top() const {
        if (m_data.empty()) {
            throw std::underflow_error("CustomStack::top() called on empty stack");
        }
        return m_data.back();
    }

    // =========================================================================
    // Capacity & State
    // =========================================================================

    size_type size() const noexcept { return m_data.size(); }
    bool empty() const noexcept { return m_data.empty(); }
    void clear() noexcept { m_data.clear(); }
    void reserve(size_type newCap) { m_data.reserve(newCap); }

    void swap(CustomStack& other) noexcept {
        m_data.swap(other.m_data);
    }

private:
    CustomDynamicArray<T> m_data;
};

} // namespace sse::dsa

namespace sse {
    template <typename T>
    using Stack = sse::dsa::CustomStack<T>;
}
