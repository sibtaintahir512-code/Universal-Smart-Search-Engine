#pragma once

#include <cstddef>
#include <utility>
#include <stdexcept>

namespace sse::dsa {

template <typename T>
struct ListNode {
    T            data;
    ListNode<T>* next{nullptr};

    ListNode(const T& val) : data(val), next(nullptr) {}
    ListNode(T&& val) : data(std::move(val)), next(nullptr) {}
};

/**
 * @brief High-performance generic singly linked list with head and tail pointers.
 * 
 * Replaces std::list / std::forward_list without relying on STL containers.
 * Guarantees O(1) push_back, push_front, and pop_front.
 * Iterative deallocation prevents call-stack overflow on deep lists (> 100k nodes).
 */
template <typename T>
class CustomSinglyLinkedList {
public:
    using value_type      = T;
    using size_type       = std::size_t;
    using reference       = T&;
    using const_reference = const T&;

    // Forward Iterator
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = T*;
        using reference         = T&;

        explicit Iterator(ListNode<T>* node) noexcept : m_current(node) {}

        reference operator*() const noexcept { return m_current->data; }
        pointer operator->() const noexcept { return &m_current->data; }

        Iterator& operator++() noexcept {
            if (m_current) m_current = m_current->next;
            return *this;
        }

        Iterator operator++(int) noexcept {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }

        bool operator==(const Iterator& other) const noexcept { return m_current == other.m_current; }
        bool operator!=(const Iterator& other) const noexcept { return m_current != other.m_current; }

        ListNode<T>* getNode() const noexcept { return m_current; }

    private:
        ListNode<T>* m_current;
    };

    class ConstIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const T*;
        using reference         = const T&;

        explicit ConstIterator(const ListNode<T>* node) noexcept : m_current(node) {}

        reference operator*() const noexcept { return m_current->data; }
        pointer operator->() const noexcept { return &m_current->data; }

        ConstIterator& operator++() noexcept {
            if (m_current) m_current = m_current->next;
            return *this;
        }

        ConstIterator operator++(int) noexcept {
            ConstIterator temp = *this;
            ++(*this);
            return temp;
        }

        bool operator==(const ConstIterator& other) const noexcept { return m_current == other.m_current; }
        bool operator!=(const ConstIterator& other) const noexcept { return m_current != other.m_current; }

    private:
        const ListNode<T>* m_current;
    };

    CustomSinglyLinkedList() noexcept
        : m_head(nullptr), m_tail(nullptr), m_size(0) {}

    ~CustomSinglyLinkedList() noexcept {
        clear();
    }

    /**
     * @brief Deep copy constructor.
     */
    CustomSinglyLinkedList(const CustomSinglyLinkedList& other)
        : m_head(nullptr), m_tail(nullptr), m_size(0) {
        for (const auto& item : other) {
            push_back(item);
        }
    }

    /**
     * @brief Move constructor. Transfers node chain in O(1).
     */
    CustomSinglyLinkedList(CustomSinglyLinkedList&& other) noexcept
        : m_head(other.m_head), m_tail(other.m_tail), m_size(other.m_size) {
        other.m_head = nullptr;
        other.m_tail = nullptr;
        other.m_size = 0;
    }

    /**
     * @brief Copy assignment operator.
     */
    CustomSinglyLinkedList& operator=(const CustomSinglyLinkedList& other) {
        if (this != &other) {
            CustomSinglyLinkedList temp(other);
            swap(temp);
        }
        return *this;
    }

    /**
     * @brief Move assignment operator.
     */
    CustomSinglyLinkedList& operator=(CustomSinglyLinkedList&& other) noexcept {
        if (this != &other) {
            clear();
            m_head = other.m_head;
            m_tail = other.m_tail;
            m_size = other.m_size;

            other.m_head = nullptr;
            other.m_tail = nullptr;
            other.m_size = 0;
        }
        return *this;
    }

    // =========================================================================
    // Element Access
    // =========================================================================

    reference front() {
        if (empty()) {
            throw std::out_of_range("CustomSinglyLinkedList::front: List is empty");
        }
        return m_head->data;
    }

    const_reference front() const {
        if (empty()) {
            throw std::out_of_range("CustomSinglyLinkedList::front: List is empty");
        }
        return m_head->data;
    }

    reference back() {
        if (empty()) {
            throw std::out_of_range("CustomSinglyLinkedList::back: List is empty");
        }
        return m_tail->data;
    }

    const_reference back() const {
        if (empty()) {
            throw std::out_of_range("CustomSinglyLinkedList::back: List is empty");
        }
        return m_tail->data;
    }

    // =========================================================================
    // Capacity
    // =========================================================================

    bool empty() const noexcept { return m_size == 0; }
    size_type size() const noexcept { return m_size; }

    // =========================================================================
    // Modifiers
    // =========================================================================

    /**
     * @brief Inserts element at head in O(1).
     */
    void push_front(const T& value) {
        auto* newNode = new ListNode<T>(value);
        newNode->next = m_head;
        m_head = newNode;
        if (!m_tail) {
            m_tail = m_head;
        }
        ++m_size;
    }

    void push_front(T&& value) {
        auto* newNode = new ListNode<T>(std::move(value));
        newNode->next = m_head;
        m_head = newNode;
        if (!m_tail) {
            m_tail = m_head;
        }
        ++m_size;
    }

    /**
     * @brief Inserts element at tail in O(1).
     */
    void push_back(const T& value) {
        auto* newNode = new ListNode<T>(value);
        if (!m_tail) {
            m_head = m_tail = newNode;
        } else {
            m_tail->next = newNode;
            m_tail = newNode;
        }
        ++m_size;
    }

    void push_back(T&& value) {
        auto* newNode = new ListNode<T>(std::move(value));
        if (!m_tail) {
            m_head = m_tail = newNode;
        } else {
            m_tail->next = newNode;
            m_tail = newNode;
        }
        ++m_size;
    }

    /**
     * @brief Removes element from head in O(1).
     */
    void pop_front() {
        if (empty()) {
            throw std::out_of_range("CustomSinglyLinkedList::pop_front: List is empty");
        }
        ListNode<T>* oldHead = m_head;
        m_head = m_head->next;
        delete oldHead;
        --m_size;
        if (m_size == 0) {
            m_tail = nullptr;
        }
    }

    /**
     * @brief Removes all elements matching predicate. Returns number of removed elements.
     */
    template <typename Predicate>
    size_type remove_if(Predicate pred) {
        ListNode<T>* current = m_head;
        ListNode<T>* prev = nullptr;
        size_type removedCount = 0;

        while (current) {
            if (pred(current->data)) {
                ListNode<T>* toDelete = current;
                if (!prev) {
                    m_head = current->next;
                } else {
                    prev->next = current->next;
                }

                if (toDelete == m_tail) {
                    m_tail = prev;
                }

                current = current->next;
                delete toDelete;
                --m_size;
                ++removedCount;
            } else {
                prev = current;
                current = current->next;
            }
        }
        return removedCount;
    }

    /**
     * @brief Removes first occurrence of value. Returns true if removed.
     */
    bool remove(const T& value) {
        return remove_if([&value](const T& item) { return item == value; });
    }

    /**
     * @brief Destructs all elements iteratively in O(N).
     */
    void clear() noexcept {
        ListNode<T>* current = m_head;
        while (current) {
            ListNode<T>* next = current->next;
            delete current;
            current = next;
        }
        m_head = nullptr;
        m_tail = nullptr;
        m_size = 0;
    }

    void swap(CustomSinglyLinkedList& other) noexcept {
        std::swap(m_head, other.m_head);
        std::swap(m_tail, other.m_tail);
        std::swap(m_size, other.m_size);
    }

    // =========================================================================
    // Iterators
    // =========================================================================
    Iterator begin() noexcept { return Iterator(m_head); }
    ConstIterator begin() const noexcept { return ConstIterator(m_head); }
    ConstIterator cbegin() const noexcept { return ConstIterator(m_head); }

    Iterator end() noexcept { return Iterator(nullptr); }
    ConstIterator end() const noexcept { return ConstIterator(nullptr); }
    ConstIterator cend() const noexcept { return ConstIterator(nullptr); }

    ListNode<T>* head() noexcept { return m_head; }
    const ListNode<T>* head() const noexcept { return m_head; }
    ListNode<T>* tail() noexcept { return m_tail; }
    const ListNode<T>* tail() const noexcept { return m_tail; }

private:
    ListNode<T>* m_head;
    ListNode<T>* m_tail;
    size_type    m_size;
};

} // namespace sse::dsa

namespace sse {
    template <typename T>
    using SinglyLinkedList = sse::dsa::CustomSinglyLinkedList<T>;
}
