#pragma once

#include <cstddef>
#include <stdexcept>
#include <utility>

namespace sse::dsa {

/**
 * @brief Node for singly linked CustomQueue.
 */
template <typename T>
struct QueueNode {
    T data;
    QueueNode* next{nullptr};

    template <typename... Args>
    explicit QueueNode(Args&&... args)
        : data(std::forward<Args>(args)...), next(nullptr) {}
};

/**
 * @brief FIFO Queue implemented via custom singly linked list with head and tail pointers.
 * 
 * Used for:
 * 1. Breadth-First Search (BFS) directory traversal in FileScanner.
 * 2. Token stream pipelining from QueryParser to BooleanSearchEngine.
 * 
 * All enqueue and dequeue operations run in deterministic O(1) time.
 */
template <typename T>
class CustomQueue {
public:
    using size_type = std::size_t;
    using value_type = T;

    CustomQueue() noexcept = default;

    ~CustomQueue() noexcept {
        clear();
    }

    // Rule of Five
    CustomQueue(const CustomQueue& other) {
        QueueNode<T>* curr = other.m_head;
        while (curr) {
            enqueue(curr->data);
            curr = curr->next;
        }
    }

    CustomQueue(CustomQueue&& other) noexcept
        : m_head(other.m_head), m_tail(other.m_tail), m_size(other.m_size) {
        other.m_head = nullptr;
        other.m_tail = nullptr;
        other.m_size = 0;
    }

    CustomQueue& operator=(const CustomQueue& other) {
        if (this != &other) {
            CustomQueue temp(other);
            swap(temp);
        }
        return *this;
    }

    CustomQueue& operator=(CustomQueue&& other) noexcept {
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

    /**
     * @brief Inserts an element at the back of the queue. O(1).
     */
    void enqueue(const T& value) {
        auto* newNode = new QueueNode<T>(value);
        if (!m_tail) {
            m_head = m_tail = newNode;
        } else {
            m_tail->next = newNode;
            m_tail = newNode;
        }
        ++m_size;
    }

    void enqueue(T&& value) {
        auto* newNode = new QueueNode<T>(std::move(value));
        if (!m_tail) {
            m_head = m_tail = newNode;
        } else {
            m_tail->next = newNode;
            m_tail = newNode;
        }
        ++m_size;
    }

    template <typename... Args>
    void emplace(Args&&... args) {
        auto* newNode = new QueueNode<T>(std::forward<Args>(args)...);
        if (!m_tail) {
            m_head = m_tail = newNode;
        } else {
            m_tail->next = newNode;
            m_tail = newNode;
        }
        ++m_size;
    }

    void push(const T& value) { enqueue(value); }
    void push(T&& value) { enqueue(std::move(value)); }

    /**
     * @brief Removes the front element from the queue. O(1).
     */
    void dequeue() {
        if (!m_head) {
            throw std::underflow_error("CustomQueue::dequeue() called on empty queue");
        }
        QueueNode<T>* temp = m_head;
        m_head = m_head->next;
        if (!m_head) {
            m_tail = nullptr;
        }
        delete temp;
        --m_size;
    }

    void pop() { dequeue(); }

    /**
     * @brief Dequeues and returns the front element. O(1).
     */
    T popFront() {
        if (!m_head) {
            throw std::underflow_error("CustomQueue::popFront() called on empty queue");
        }
        T val = std::move(m_head->data);
        dequeue();
        return val;
    }

    /**
     * @brief Returns a reference to the front element. O(1).
     */
    T& front() {
        if (!m_head) {
            throw std::underflow_error("CustomQueue::front() called on empty queue");
        }
        return m_head->data;
    }

    const T& front() const {
        if (!m_head) {
            throw std::underflow_error("CustomQueue::front() called on empty queue");
        }
        return m_head->data;
    }

    /**
     * @brief Returns a reference to the back element. O(1).
     */
    T& back() {
        if (!m_tail) {
            throw std::underflow_error("CustomQueue::back() called on empty queue");
        }
        return m_tail->data;
    }

    const T& back() const {
        if (!m_tail) {
            throw std::underflow_error("CustomQueue::back() called on empty queue");
        }
        return m_tail->data;
    }

    // =========================================================================
    // Capacity & State
    // =========================================================================

    size_type size() const noexcept { return m_size; }
    bool empty() const noexcept { return m_head == nullptr; }

    void clear() noexcept {
        QueueNode<T>* curr = m_head;
        while (curr) {
            QueueNode<T>* next = curr->next;
            delete curr;
            curr = next;
        }
        m_head = nullptr;
        m_tail = nullptr;
        m_size = 0;
    }

    void swap(CustomQueue& other) noexcept {
        std::swap(m_head, other.m_head);
        std::swap(m_tail, other.m_tail);
        std::swap(m_size, other.m_size);
    }

private:
    QueueNode<T>* m_head{nullptr};
    QueueNode<T>* m_tail{nullptr};
    size_type     m_size{0};
};

} // namespace sse::dsa

namespace sse {
    template <typename T>
    using Queue = sse::dsa::CustomQueue<T>;
}
