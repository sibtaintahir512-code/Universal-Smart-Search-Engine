#pragma once

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>
#include "dsa/CustomDynamicArray.hpp"

namespace sse::dsa {

/**
 * @brief Binary Max-Heap backed by contiguous CustomDynamicArray<T>.
 * 
 * Supports:
 * - O(1) peekTop / top
 * - O(log N) insert / push
 * - O(log N) extractTop / pop
 * - O(log K) replaceTop (for Bounded Min/Max Heap online filtering)
 * - O(N) Floyd's linear buildHeap
 * 
 * Default Compare is std::less<T>, which ensures the maximum element sits at root.
 */
template <typename T, typename Compare = std::less<T>>
class CustomMaxHeap {
public:
    using size_type = std::size_t;
    using value_type = T;

    explicit CustomMaxHeap(Compare comp = Compare()) : m_comp(std::move(comp)) {}

    CustomMaxHeap(const CustomDynamicArray<T>& data, Compare comp = Compare())
        : m_data(data), m_comp(std::move(comp)) {
        buildHeap();
    }

    CustomMaxHeap(CustomDynamicArray<T>&& data, Compare comp = Compare())
        : m_data(std::move(data)), m_comp(std::move(comp)) {
        buildHeap();
    }

    ~CustomMaxHeap() = default;

    // Rule of Five
    CustomMaxHeap(const CustomMaxHeap& other) = default;
    CustomMaxHeap(CustomMaxHeap&& other) noexcept = default;
    CustomMaxHeap& operator=(const CustomMaxHeap& other) = default;
    CustomMaxHeap& operator=(CustomMaxHeap&& other) noexcept = default;

    /**
     * @brief Pushes a new element into the heap. O(log N).
     */
    void push(const T& value) {
        m_data.push_back(value);
        heapifyUp(m_data.size() - 1);
    }

    void push(T&& value) {
        m_data.push_back(std::move(value));
        heapifyUp(m_data.size() - 1);
    }

    void insert(const T& value) { push(value); }
    void insert(T&& value) { push(std::move(value)); }

    /**
     * @brief Returns a reference to the top (maximum) element. O(1).
     */
    const T& top() const {
        if (m_data.empty()) {
            throw std::underflow_error("CustomMaxHeap::top() called on empty heap");
        }
        return m_data[0];
    }

    const T& peekTop() const { return top(); }

    /**
     * @brief Removes the top (maximum) element from the heap. O(log N).
     */
    void pop() {
        if (m_data.empty()) {
            throw std::underflow_error("CustomMaxHeap::pop() called on empty heap");
        }
        if (m_data.size() == 1) {
            m_data.pop_back();
            return;
        }
        m_data[0] = std::move(m_data[m_data.size() - 1]);
        m_data.pop_back();
        heapifyDown(0);
    }

    /**
     * @brief Extracts and returns the top (maximum) element. O(log N).
     */
    T extractTop() {
        if (m_data.empty()) {
            throw std::underflow_error("CustomMaxHeap::extractTop() called on empty heap");
        }
        T topItem = std::move(m_data[0]);
        if (m_data.size() == 1) {
            m_data.pop_back();
        } else {
            m_data[0] = std::move(m_data[m_data.size() - 1]);
            m_data.pop_back();
            heapifyDown(0);
        }
        return topItem;
    }

    T extractMax() { return extractTop(); }

    /**
     * @brief Replaces the top element with newValue and restores the heap property.
     * Operates in O(log N) with zero buffer reallocation.
     */
    void replaceTop(const T& newValue) {
        if (m_data.empty()) {
            push(newValue);
            return;
        }
        m_data[0] = newValue;
        heapifyDown(0);
    }

    void replaceTop(T&& newValue) {
        if (m_data.empty()) {
            push(std::move(newValue));
            return;
        }
        m_data[0] = std::move(newValue);
        heapifyDown(0);
    }

    /**
     * @brief Reconstructs the heap property in-place using Floyd's linear O(N) algorithm.
     */
    void buildHeap() {
        if (m_data.size() <= 1) {
            return;
        }
        for (std::size_t i = m_data.size() / 2; i > 0; --i) {
            heapifyDown(i - 1);
        }
    }

    /**
     * @brief Clears and repopulates heap from input array, calling linear buildHeap.
     */
    void buildFrom(const CustomDynamicArray<T>& data) {
        m_data = data;
        buildHeap();
    }

    void buildFrom(CustomDynamicArray<T>&& data) {
        m_data = std::move(data);
        buildHeap();
    }

    // =========================================================================
    // Capacity & State
    // =========================================================================

    size_type size() const noexcept { return m_data.size(); }
    size_type capacity() const noexcept { return m_data.capacity(); }
    bool empty() const noexcept { return m_data.empty(); }

    void reserve(size_type newCap) { m_data.reserve(newCap); }
    void clear() noexcept { m_data.clear(); }

    const CustomDynamicArray<T>& data() const noexcept { return m_data; }
    CustomDynamicArray<T>& data() noexcept { return m_data; }

private:
    CustomDynamicArray<T> m_data;
    Compare               m_comp;

    static constexpr size_type parentIndex(size_type i) noexcept {
        return (i - 1) / 2;
    }

    static constexpr size_type leftChildIndex(size_type i) noexcept {
        return 2 * i + 1;
    }

    static constexpr size_type rightChildIndex(size_type i) noexcept {
        return 2 * i + 2;
    }

    void heapifyUp(size_type i) {
        while (i > 0) {
            size_type p = parentIndex(i);
            // If parent < current according to m_comp, swap to maintain max-heap
            if (m_comp(m_data[p], m_data[i])) {
                std::swap(m_data[p], m_data[i]);
                i = p;
            } else {
                break;
            }
        }
    }

    void heapifyDown(size_type i) {
        const size_type n = m_data.size();
        while (true) {
            size_type largest = i;
            size_type left = leftChildIndex(i);
            size_type right = rightChildIndex(i);

            if (left < n && m_comp(m_data[largest], m_data[left])) {
                largest = left;
            }
            if (right < n && m_comp(m_data[largest], m_data[right])) {
                largest = right;
            }

            if (largest != i) {
                std::swap(m_data[i], m_data[largest]);
                i = largest;
            } else {
                break;
            }
        }
    }
};

} // namespace sse::dsa

namespace sse {
    template <typename T, typename Compare = std::less<T>>
    using MaxHeap = sse::dsa::CustomMaxHeap<T, Compare>;
}
