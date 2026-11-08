#pragma once

#include <cstddef>
#include <new>
#include <utility>
#include <stdexcept>
#include <algorithm>

namespace sse::dsa {

/**
 * @brief High-performance generic dynamic array with 1.5x geometric expansion.
 * 
 * Replaces std::vector without relying on STL container abstractions.
 * Uses raw byte buffers, placement new for element construction,
 * and explicit destructor invocations.
 */
template <typename T>
class CustomDynamicArray {
public:
    using value_type      = T;
    using size_type       = std::size_t;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;

    // Iterator support
    using iterator        = T*;
    using const_iterator  = const T*;

    /**
     * @brief Constructs an empty dynamic array with zero initial allocation.
     */
    CustomDynamicArray() noexcept
        : m_data(nullptr), m_size(0), m_capacity(0) {}

    /**
     * @brief Constructs array with specified initial capacity reserved.
     */
    explicit CustomDynamicArray(size_type initialCapacity)
        : m_data(nullptr), m_size(0), m_capacity(0) {
        if (initialCapacity > 0) {
            reserve(initialCapacity);
        }
    }

    /**
     * @brief Constructs array with `count` default-initialized elements.
     */
    CustomDynamicArray(size_type count, const T& value)
        : m_data(nullptr), m_size(0), m_capacity(0) {
        if (count > 0) {
            reserve(count);
            for (size_type i = 0; i < count; ++i) {
                ::new (static_cast<void*>(m_data + i)) T(value);
            }
            m_size = count;
        }
    }

    /**
     * @brief Destructor. Destructs all live objects and deallocates raw storage.
     */
    ~CustomDynamicArray() noexcept {
        destroyElements();
        deallocateBuffer();
    }

    /**
     * @brief Deep copy constructor.
     */
    CustomDynamicArray(const CustomDynamicArray& other)
        : m_data(nullptr), m_size(0), m_capacity(0) {
        if (other.m_size > 0) {
            reserve(other.m_size);
            for (size_type i = 0; i < other.m_size; ++i) {
                ::new (static_cast<void*>(m_data + i)) T(other.m_data[i]);
            }
            m_size = other.m_size;
        }
    }

    /**
     * @brief Move constructor. Transfers buffer ownership in O(1).
     */
    CustomDynamicArray(CustomDynamicArray&& other) noexcept
        : m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity) {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    /**
     * @brief Copy assignment operator using copy-and-swap idiom.
     */
    CustomDynamicArray& operator=(const CustomDynamicArray& other) {
        if (this != &other) {
            CustomDynamicArray temp(other);
            swap(temp);
        }
        return *this;
    }

    /**
     * @brief Move assignment operator.
     */
    CustomDynamicArray& operator=(CustomDynamicArray&& other) noexcept {
        if (this != &other) {
            destroyElements();
            deallocateBuffer();

            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;

            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    // =========================================================================
    // Element Access
    // =========================================================================

    reference operator[](size_type index) noexcept {
        return m_data[index];
    }

    const_reference operator[](size_type index) const noexcept {
        return m_data[index];
    }

    reference at(size_type index) {
        if (index >= m_size) {
            throw std::out_of_range("CustomDynamicArray::at: Index out of range");
        }
        return m_data[index];
    }

    const_reference at(size_type index) const {
        if (index >= m_size) {
            throw std::out_of_range("CustomDynamicArray::at: Index out of range");
        }
        return m_data[index];
    }

    reference front() {
        if (empty()) {
            throw std::out_of_range("CustomDynamicArray::front: Array is empty");
        }
        return m_data[0];
    }

    const_reference front() const {
        if (empty()) {
            throw std::out_of_range("CustomDynamicArray::front: Array is empty");
        }
        return m_data[0];
    }

    reference back() {
        if (empty()) {
            throw std::out_of_range("CustomDynamicArray::back: Array is empty");
        }
        return m_data[m_size - 1];
    }

    const_reference back() const {
        if (empty()) {
            throw std::out_of_range("CustomDynamicArray::back: Array is empty");
        }
        return m_data[m_size - 1];
    }

    pointer data() noexcept { return m_data; }
    const_pointer data() const noexcept { return m_data; }

    // =========================================================================
    // Capacity & Size
    // =========================================================================

    bool empty() const noexcept { return m_size == 0; }
    size_type size() const noexcept { return m_size; }
    size_type capacity() const noexcept { return m_capacity; }

    /**
     * @brief Reserves buffer capacity for at least `newCapacity` elements.
     */
    void reserve(size_type newCapacity) {
        if (newCapacity <= m_capacity) {
            return;
        }
        reallocate(newCapacity);
    }

    /**
     * @brief Shrinks allocated buffer to fit active size.
     */
    void shrink_to_fit() {
        if (m_capacity > m_size) {
            if (m_size == 0) {
                destroyElements();
                deallocateBuffer();
                m_capacity = 0;
            } else {
                reallocate(m_size);
            }
        }
    }

    // =========================================================================
    // Modifiers
    // =========================================================================

    /**
     * @brief Destructs all elements without freeing buffer memory.
     */
    void clear() noexcept {
        destroyElements();
        m_size = 0;
    }

    /**
     * @brief Appends copy of value to end of array.
     */
    void push_back(const T& value) {
        ensureCapacity();
        ::new (static_cast<void*>(m_data + m_size)) T(value);
        ++m_size;
    }

    /**
     * @brief Appends moved value to end of array.
     */
    void push_back(T&& value) {
        ensureCapacity();
        ::new (static_cast<void*>(m_data + m_size)) T(std::move(value));
        ++m_size;
    }

    /**
     * @brief Constructs element in-place at the end.
     */
    template <typename... Args>
    reference emplace_back(Args&&... args) {
        ensureCapacity();
        T* slot = ::new (static_cast<void*>(m_data + m_size)) T(std::forward<Args>(args)...);
        ++m_size;
        return *slot;
    }

    /**
     * @brief Removes the last element.
     */
    void pop_back() {
        if (empty()) {
            throw std::out_of_range("CustomDynamicArray::pop_back: Array is empty");
        }
        --m_size;
        m_data[m_size].~T();
    }

    /**
     * @brief Inserts element at given index.
     */
    void insert(size_type index, const T& value) {
        if (index > m_size) {
            throw std::out_of_range("CustomDynamicArray::insert: Index out of range");
        }
        ensureCapacity();
        // Construct new element at end
        if (index == m_size) {
            ::new (static_cast<void*>(m_data + m_size)) T(value);
        } else {
            ::new (static_cast<void*>(m_data + m_size)) T(std::move(m_data[m_size - 1]));
            for (size_type i = m_size - 1; i > index; --i) {
                m_data[i] = std::move(m_data[i - 1]);
            }
            m_data[index] = value;
        }
        ++m_size;
    }

    /**
     * @brief Removes element at given index.
     */
    void erase(size_type index) {
        if (index >= m_size) {
            throw std::out_of_range("CustomDynamicArray::erase: Index out of range");
        }
        for (size_type i = index; i + 1 < m_size; ++i) {
            m_data[i] = std::move(m_data[i + 1]);
        }
        --m_size;
        m_data[m_size].~T();
    }

    /**
     * @brief Swaps contents with another array.
     */
    void swap(CustomDynamicArray& other) noexcept {
        std::swap(m_data, other.m_data);
        std::swap(m_size, other.m_size);
        std::swap(m_capacity, other.m_capacity);
    }

    // =========================================================================
    // Iterators
    // =========================================================================
    iterator begin() noexcept { return m_data; }
    const_iterator begin() const noexcept { return m_data; }
    const_iterator cbegin() const noexcept { return m_data; }

    iterator end() noexcept { return m_data + m_size; }
    const_iterator end() const noexcept { return m_data + m_size; }
    const_iterator cend() const noexcept { return m_data + m_size; }

private:
    T*        m_data;
    size_type m_size;
    size_type m_capacity;

    void ensureCapacity() {
        if (m_size >= m_capacity) {
            // 1.5x geometric expansion formula
            size_type nextCapacity = (m_capacity == 0) ? 8 : (m_capacity + m_capacity / 2);
            reallocate(nextCapacity);
        }
    }

    void reallocate(size_type newCapacity) {
        T* newBuffer = static_cast<T*>(::operator new(newCapacity * sizeof(T)));

        for (size_type i = 0; i < m_size; ++i) {
            ::new (static_cast<void*>(newBuffer + i)) T(std::move(m_data[i]));
            m_data[i].~T();
        }

        deallocateBuffer();
        m_data = newBuffer;
        m_capacity = newCapacity;
    }

    void destroyElements() noexcept {
        for (size_type i = 0; i < m_size; ++i) {
            m_data[i].~T();
        }
    }

    void deallocateBuffer() noexcept {
        if (m_data) {
            ::operator delete(static_cast<void*>(m_data));
            m_data = nullptr;
        }
    }
};

} // namespace sse::dsa

// Backward-compatible alias
namespace sse {
    template <typename T>
    using DynamicArray = sse::dsa::CustomDynamicArray<T>;
}
