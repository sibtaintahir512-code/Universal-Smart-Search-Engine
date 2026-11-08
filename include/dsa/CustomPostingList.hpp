#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include "core/CommonTypes.hpp"

namespace sse::dsa {

/**
 * @brief Ordered linked list of PostingNodes maintaining strict ascending DocID sequence.
 * 
 * Central structure for Inverted Index posting chains.
 * Implements linear two-pointer set operations (Intersect, Union, Difference)
 * in O(L1 + L2) time.
 */
class CustomPostingList {
public:
    using size_type = std::size_t;

    CustomPostingList() noexcept;
    ~CustomPostingList() noexcept;

    // Rule of Five
    CustomPostingList(const CustomPostingList& other);
    CustomPostingList(CustomPostingList&& other) noexcept;
    CustomPostingList& operator=(const CustomPostingList& other);
    CustomPostingList& operator=(CustomPostingList&& other) noexcept;

    /**
     * @brief Appends or updates a posting occurrence for given DocID.
     * Guaranteed O(1) during monotonic document ingestion.
     */
    void addOccurrence(DocID docId, bool isFilename);

    /**
     * @brief Directly appends a new node with specific frequencies.
     */
    void appendNode(DocID docId, uint16_t nameFreq, uint16_t bodyFreq, uint8_t fieldMask);

    /**
     * @brief Linear two-pointer set intersection (AND).
     * Time: O(L_A + L_B), Space: O(min(L_A, L_B)).
     */
    static CustomPostingList intersect(const CustomPostingList* a, const CustomPostingList* b);

    /**
     * @brief Linear two-pointer set union (OR).
     * Time: O(L_A + L_B), Space: O(L_A + L_B).
     */
    static CustomPostingList unionLists(const CustomPostingList* a, const CustomPostingList* b);

    /**
     * @brief Linear two-pointer set difference (NOT).
     * Computes (universe \ exclude).
     * Time: O(L_univ + L_excl), Space: O(L_univ).
     */
    static CustomPostingList difference(const CustomPostingList* universe, const CustomPostingList* exclude);

    // =========================================================================
    // Accessors
    // =========================================================================

    const PostingNode* head() const noexcept { return m_head; }
    PostingNode* head() noexcept { return m_head; }

    const PostingNode* tail() const noexcept { return m_tail; }
    PostingNode* tail() noexcept { return m_tail; }

    uint32_t documentFrequency() const noexcept { return m_docFreq; }
    size_type size() const noexcept { return static_cast<size_type>(m_docFreq); }
    bool empty() const noexcept { return m_head == nullptr; }

    void clear() noexcept;
    void swap(CustomPostingList& other) noexcept;

private:
    PostingNode* m_head{nullptr};
    PostingNode* m_tail{nullptr};
    uint32_t     m_docFreq{0};

    void insertSorted(PostingNode* newNode);
};

} // namespace sse::dsa

namespace sse {
    using PostingList = sse::dsa::CustomPostingList;
}
