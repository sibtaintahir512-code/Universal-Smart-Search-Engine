#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <stdexcept>
#include <type_traits>

namespace sse::dsa {

/**
 * @brief 64-bit FNV-1a hash function implementation.
 * Operates byte-by-byte, fully supporting arbitrary UTF-8 multi-byte sequences.
 */
struct Fnv1aHash {
    static constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    static constexpr uint64_t FNV_PRIME        = 1099511628211ULL;

    uint64_t operator()(std::string_view key) const noexcept {
        uint64_t hash = FNV_OFFSET_BASIS;
        for (unsigned char c : key) {
            hash ^= static_cast<uint64_t>(c);
            hash *= FNV_PRIME;
        }
        return hash;
    }

    uint64_t operator()(const std::string& key) const noexcept {
        return (*this)(std::string_view(key));
    }

    template <typename Integral, typename = std::enable_if_t<std::is_integral_v<Integral>>>
    uint64_t operator()(Integral key) const noexcept {
        uint64_t hash = FNV_OFFSET_BASIS;
        const auto* bytes = reinterpret_cast<const unsigned char*>(&key);
        for (std::size_t i = 0; i < sizeof(Integral); ++i) {
            hash ^= static_cast<uint64_t>(bytes[i]);
            hash *= FNV_PRIME;
        }
        return hash;
    }
};

template <typename K, typename V>
struct HashEntry {
    K                 key;
    V                 value;
    HashEntry<K, V>*  next{nullptr};

    HashEntry(const K& k, const V& v) : key(k), value(v), next(nullptr) {}
    HashEntry(const K& k, V&& v) : key(k), value(std::move(v)), next(nullptr) {}
};

/**
 * @brief High-performance custom Hash Table utilizing separate chaining.
 * 
 * Replaces std::unordered_map without relying on STL container abstractions.
 * Features 64-bit FNV-1a hashing, power-of-two bucket masking, and
 * dynamic rehashing at load factor alpha > 0.75 via node splicing.
 */
template <typename K, typename V, typename Hash = Fnv1aHash>
class CustomHashTable {
public:
    using key_type        = K;
    using mapped_type     = V;
    using size_type       = std::size_t;

    static constexpr size_type DEFAULT_INITIAL_BUCKETS = 16;
    static constexpr double    MAX_LOAD_FACTOR         = 0.75;

    explicit CustomHashTable(size_type initialBuckets = DEFAULT_INITIAL_BUCKETS)
        : m_buckets(nullptr), m_bucketCount(0), m_size(0), m_hash() {
        size_type buckets = (initialBuckets < 2) ? 2 : initialBuckets;
        // Round up to nearest power of two
        m_bucketCount = nextPowerOfTwo(buckets);
        allocateBuckets(m_bucketCount);
    }

    ~CustomHashTable() noexcept {
        clear();
        deallocateBuckets();
    }

    /**
     * @brief Deep copy constructor.
     */
    CustomHashTable(const CustomHashTable& other)
        : m_buckets(nullptr), m_bucketCount(other.m_bucketCount), m_size(0), m_hash(other.m_hash) {
        allocateBuckets(m_bucketCount);
        for (size_type i = 0; i < other.m_bucketCount; ++i) {
            HashEntry<K, V>* current = other.m_buckets[i];
            while (current) {
                insert(current->key, current->value);
                current = current->next;
            }
        }
    }

    /**
     * @brief Move constructor. Transfers bucket array in O(1).
     */
    CustomHashTable(CustomHashTable&& other) noexcept
        : m_buckets(other.m_buckets), m_bucketCount(other.m_bucketCount),
          m_size(other.m_size), m_hash(std::move(other.m_hash)) {
        other.m_buckets = nullptr;
        other.m_bucketCount = 0;
        other.m_size = 0;
    }

    /**
     * @brief Copy assignment operator.
     */
    CustomHashTable& operator=(const CustomHashTable& other) {
        if (this != &other) {
            CustomHashTable temp(other);
            swap(temp);
        }
        return *this;
    }

    /**
     * @brief Move assignment operator.
     */
    CustomHashTable& operator=(CustomHashTable&& other) noexcept {
        if (this != &other) {
            clear();
            deallocateBuckets();

            m_buckets = other.m_buckets;
            m_bucketCount = other.m_bucketCount;
            m_size = other.m_size;
            m_hash = std::move(other.m_hash);

            other.m_buckets = nullptr;
            other.m_bucketCount = 0;
            other.m_size = 0;
        }
        return *this;
    }

    // =========================================================================
    // Lookup & Access
    // =========================================================================

    V* find(const K& key) noexcept {
        if (m_bucketCount == 0) return nullptr;
        size_type bucketIndex = getBucketIndex(key);
        HashEntry<K, V>* current = m_buckets[bucketIndex];
        while (current) {
            if (current->key == key) {
                return &current->value;
            }
            current = current->next;
        }
        return nullptr;
    }

    const V* find(const K& key) const noexcept {
        if (m_bucketCount == 0) return nullptr;
        size_type bucketIndex = getBucketIndex(key);
        const HashEntry<K, V>* current = m_buckets[bucketIndex];
        while (current) {
            if (current->key == key) {
                return &current->value;
            }
            current = current->next;
        }
        return nullptr;
    }

    bool contains(const K& key) const noexcept {
        return find(key) != nullptr;
    }

    V& operator[](const K& key) {
        checkAndRehash();
        size_type bucketIndex = getBucketIndex(key);
        HashEntry<K, V>* current = m_buckets[bucketIndex];
        while (current) {
            if (current->key == key) {
                return current->value;
            }
            current = current->next;
        }

        // Key not found: insert default constructed value at bucket head
        auto* newEntry = new HashEntry<K, V>(key, V());
        newEntry->next = m_buckets[bucketIndex];
        m_buckets[bucketIndex] = newEntry;
        ++m_size;
        return newEntry->value;
    }

    // =========================================================================
    // Modifiers
    // =========================================================================

    /**
     * @brief Inserts or updates key-value pair.
     * Returns true if newly inserted, false if existing key updated.
     */
    bool insert(const K& key, const V& value) {
        checkAndRehash();
        size_type bucketIndex = getBucketIndex(key);
        HashEntry<K, V>* current = m_buckets[bucketIndex];
        while (current) {
            if (current->key == key) {
                current->value = value;
                return false;
            }
            current = current->next;
        }

        auto* newEntry = new HashEntry<K, V>(key, value);
        newEntry->next = m_buckets[bucketIndex];
        m_buckets[bucketIndex] = newEntry;
        ++m_size;
        return true;
    }

    bool insert(const K& key, V&& value) {
        checkAndRehash();
        size_type bucketIndex = getBucketIndex(key);
        HashEntry<K, V>* current = m_buckets[bucketIndex];
        while (current) {
            if (current->key == key) {
                current->value = std::move(value);
                return false;
            }
            current = current->next;
        }

        auto* newEntry = new HashEntry<K, V>(key, std::move(value));
        newEntry->next = m_buckets[bucketIndex];
        m_buckets[bucketIndex] = newEntry;
        ++m_size;
        return true;
    }

    /**
     * @brief Removes key from table. Returns true if removed, false if not found.
     */
    bool remove(const K& key) noexcept {
        if (m_bucketCount == 0) return false;
        size_type bucketIndex = getBucketIndex(key);
        HashEntry<K, V>* current = m_buckets[bucketIndex];
        HashEntry<K, V>* prev = nullptr;

        while (current) {
            if (current->key == key) {
                if (!prev) {
                    m_buckets[bucketIndex] = current->next;
                } else {
                    prev->next = current->next;
                }
                delete current;
                --m_size;
                return true;
            }
            prev = current;
            current = current->next;
        }
        return false;
    }

    bool erase(const K& key) noexcept {
        return remove(key);
    }

    /**
     * @brief Clears all entries while retaining current bucket allocation.
     */
    void clear() noexcept {
        if (!m_buckets) return;
        for (size_type i = 0; i < m_bucketCount; ++i) {
            HashEntry<K, V>* current = m_buckets[i];
            while (current) {
                HashEntry<K, V>* next = current->next;
                delete current;
                current = next;
            }
            m_buckets[i] = nullptr;
        }
        m_size = 0;
    }

    void swap(CustomHashTable& other) noexcept {
        std::swap(m_buckets, other.m_buckets);
        std::swap(m_bucketCount, other.m_bucketCount);
        std::swap(m_size, other.m_size);
        std::swap(m_hash, other.m_hash);
    }

    // =========================================================================
    // Capacity & Telemetry
    // =========================================================================

    bool empty() const noexcept { return m_size == 0; }
    size_type size() const noexcept { return m_size; }
    size_type bucket_count() const noexcept { return m_bucketCount; }
    size_type bucketCount() const noexcept { return m_bucketCount; }

    double load_factor() const noexcept {
        if (m_bucketCount == 0) return 0.0;
        return static_cast<double>(m_size) / static_cast<double>(m_bucketCount);
    }

    size_type max_chain_length() const noexcept {
        size_type maxLen = 0;
        for (size_type i = 0; i < m_bucketCount; ++i) {
            size_type currentLen = 0;
            HashEntry<K, V>* current = m_buckets[i];
            while (current) {
                ++currentLen;
                current = current->next;
            }
            if (currentLen > maxLen) {
                maxLen = currentLen;
            }
        }
        return maxLen;
    }

    /**
     * @brief Rehashes table to new bucket count.
     */
    void rehash(size_type newBucketCount) {
        size_type targetBuckets = nextPowerOfTwo(newBucketCount);
        if (targetBuckets <= m_bucketCount) return;

        HashEntry<K, V>** newBuckets = new HashEntry<K, V>*[targetBuckets]();

        // Node splicing: re-index existing nodes into new buckets without heap re-allocation
        for (size_type i = 0; i < m_bucketCount; ++i) {
            HashEntry<K, V>* current = m_buckets[i];
            while (current) {
                HashEntry<K, V>* next = current->next;
                size_type newIndex = m_hash(current->key) & (targetBuckets - 1);

                current->next = newBuckets[newIndex];
                newBuckets[newIndex] = current;

                current = next;
            }
        }

        delete[] m_buckets;
        m_buckets = newBuckets;
        m_bucketCount = targetBuckets;
    }

    // Access to internal bucket array for advanced maintenance / iterators
    HashEntry<K, V>** getBuckets() noexcept { return m_buckets; }
    const HashEntry<K, V>* const* getBuckets() const noexcept { return m_buckets; }

private:
    HashEntry<K, V>** m_buckets;
    size_type         m_bucketCount;
    size_type         m_size;
    Hash              m_hash;

    size_type getBucketIndex(const K& key) const noexcept {
        return m_hash(key) & (m_bucketCount - 1);
    }

    void checkAndRehash() {
        if (load_factor() >= MAX_LOAD_FACTOR) {
            size_type nextBuckets = (m_bucketCount == 0) ? DEFAULT_INITIAL_BUCKETS : (m_bucketCount * 2);
            rehash(nextBuckets);
        }
    }

    void allocateBuckets(size_type count) {
        m_buckets = new HashEntry<K, V>*[count]();
    }

    void deallocateBuckets() noexcept {
        if (m_buckets) {
            delete[] m_buckets;
            m_buckets = nullptr;
            m_bucketCount = 0;
        }
    }

    static size_type nextPowerOfTwo(size_type n) noexcept {
        if (n == 0) return 1;
        --n;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        if constexpr (sizeof(size_type) > 4) {
            n |= n >> 32;
        }
        return n + 1;
    }
};

} // namespace sse::dsa

namespace sse {
    template <typename K, typename V, typename Hash = sse::dsa::Fnv1aHash>
    using HashTable = sse::dsa::CustomHashTable<K, V, Hash>;
}
