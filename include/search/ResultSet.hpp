#pragma once

#include <cstddef>
#include <cstdint>
#include "core/CommonTypes.hpp"
#include "dsa/CustomDynamicArray.hpp"

namespace sse::search {

/**
 * @brief Custom algorithmic Result Set storing unique DocIDs in strictly ascending order.
 *
 * Backed entirely by sse::dsa::CustomDynamicArray<DocID> (Zero STL container usage).
 * Implements deterministic two-pointer set operations (Union, Intersection, Difference)
 * in linear O(N + M) time and O(log N) binary search membership testing.
 */
class ResultSet {
public:
    using size_type = std::size_t;

    ResultSet() = default;
    explicit ResultSet(size_type initialCapacity);
    ~ResultSet() = default;

    // Rule of Five
    ResultSet(const ResultSet& other) = default;
    ResultSet(ResultSet&& other) noexcept = default;
    ResultSet& operator=(const ResultSet& other) = default;
    ResultSet& operator=(ResultSet&& other) noexcept = default;

    /**
     * @brief Appends or inserts a DocID, maintaining strict ascending order and rejecting duplicates.
     * O(1) amortized when appending in monotonic order.
     */
    void add(DocID docId);

    /**
     * @brief O(log N) binary search membership check.
     */
    bool contains(DocID docId) const noexcept;

    /**
     * @brief Deterministic linear two-pointer set intersection (A ∩ B).
     * Time: O(|A| + |B|), Space: O(min(|A|, |B|)).
     */
    static ResultSet intersect(const ResultSet& a, const ResultSet& b);

    /**
     * @brief Deterministic linear two-pointer set union (A ∪ B).
     * Time: O(|A| + |B|), Space: O(|A| + |B|).
     */
    static ResultSet unionSets(const ResultSet& a, const ResultSet& b);

    /**
     * @brief Deterministic linear two-pointer set difference (Universe \ Exclude).
     * Time: O(|Universe| + |Exclude|), Space: O(|Universe|).
     */
    static ResultSet difference(const ResultSet& universe, const ResultSet& exclude);

    // =========================================================================
    // Accessors & Iteration
    // =========================================================================

    size_type size() const noexcept { return m_docIds.size(); }
    bool empty() const noexcept { return m_docIds.empty(); }
    void clear() noexcept { m_docIds.clear(); }

    DocID operator[](size_type index) const { return m_docIds[index]; }
    DocID& operator[](size_type index) { return m_docIds[index]; }

    const DocID* data() const noexcept { return m_docIds.data(); }
    DocID* data() noexcept { return m_docIds.data(); }

    const DocID* begin() const noexcept { return m_docIds.begin(); }
    const DocID* end() const noexcept { return m_docIds.end(); }
    DocID* begin() noexcept { return m_docIds.begin(); }
    DocID* end() noexcept { return m_docIds.end(); }

    const dsa::CustomDynamicArray<DocID>& docIds() const noexcept { return m_docIds; }
    dsa::CustomDynamicArray<DocID>& mutableDocIds() noexcept { return m_docIds; }

private:
    dsa::CustomDynamicArray<DocID> m_docIds;
};

} // namespace sse::search
