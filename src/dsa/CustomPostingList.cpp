#include "dsa/CustomPostingList.hpp"

namespace sse::dsa {

CustomPostingList::CustomPostingList() noexcept
    : m_head(nullptr), m_tail(nullptr), m_docFreq(0) {}

CustomPostingList::~CustomPostingList() noexcept {
    clear();
}

CustomPostingList::CustomPostingList(const CustomPostingList& other)
    : m_head(nullptr), m_tail(nullptr), m_docFreq(0) {
    const PostingNode* current = other.m_head;
    while (current) {
        appendNode(current->docId, current->nameTermFreq, current->bodyTermFreq, current->fieldMask);
        current = current->next;
    }
}

CustomPostingList::CustomPostingList(CustomPostingList&& other) noexcept
    : m_head(other.m_head), m_tail(other.m_tail), m_docFreq(other.m_docFreq) {
    other.m_head = nullptr;
    other.m_tail = nullptr;
    other.m_docFreq = 0;
}

CustomPostingList& CustomPostingList::operator=(const CustomPostingList& other) {
    if (this != &other) {
        CustomPostingList temp(other);
        swap(temp);
    }
    return *this;
}

CustomPostingList& CustomPostingList::operator=(CustomPostingList&& other) noexcept {
    if (this != &other) {
        clear();
        m_head = other.m_head;
        m_tail = other.m_tail;
        m_docFreq = other.m_docFreq;

        other.m_head = nullptr;
        other.m_tail = nullptr;
        other.m_docFreq = 0;
    }
    return *this;
}

void CustomPostingList::clear() noexcept {
    PostingNode* current = m_head;
    while (current) {
        PostingNode* next = current->next;
        delete current;
        current = next;
    }
    m_head = nullptr;
    m_tail = nullptr;
    m_docFreq = 0;
}

void CustomPostingList::swap(CustomPostingList& other) noexcept {
    std::swap(m_head, other.m_head);
    std::swap(m_tail, other.m_tail);
    std::swap(m_docFreq, other.m_docFreq);
}

void CustomPostingList::appendNode(DocID docId, uint16_t nameFreq, uint16_t bodyFreq, uint8_t fieldMask) {
    auto* node = new PostingNode();
    node->docId = docId;
    node->nameTermFreq = nameFreq;
    node->bodyTermFreq = bodyFreq;
    node->fieldMask = fieldMask;
    node->next = nullptr;

    if (!m_tail || docId > m_tail->docId) {
        if (!m_tail) {
            m_head = m_tail = node;
        } else {
            m_tail->next = node;
            m_tail = node;
        }
        ++m_docFreq;
    } else {
        insertSorted(node);
    }
}

void CustomPostingList::addOccurrence(DocID docId, bool isFilename) {
    // Fast path: sequential monotonic append or update on current document
    if (m_tail && m_tail->docId == docId) {
        if (isFilename) {
            m_tail->nameTermFreq++;
            m_tail->fieldMask |= FIELD_FILENAME;
        } else {
            m_tail->bodyTermFreq++;
            m_tail->fieldMask |= FIELD_CONTENT;
        }
        return;
    }

    if (!m_tail || docId > m_tail->docId) {
        // Deterministic O(1) monotonic append
        auto* node = new PostingNode();
        node->docId = docId;
        node->nameTermFreq = isFilename ? 1 : 0;
        node->bodyTermFreq = isFilename ? 0 : 1;
        node->fieldMask = isFilename ? FIELD_FILENAME : FIELD_CONTENT;
        node->next = nullptr;

        if (!m_tail) {
            m_head = m_tail = node;
        } else {
            m_tail->next = node;
            m_tail = node;
        }
        ++m_docFreq;
    } else {
        // Fallback for out-of-order insertion
        auto* node = new PostingNode();
        node->docId = docId;
        node->nameTermFreq = isFilename ? 1 : 0;
        node->bodyTermFreq = isFilename ? 0 : 1;
        node->fieldMask = isFilename ? FIELD_FILENAME : FIELD_CONTENT;
        node->next = nullptr;
        insertSorted(node);
    }
}

void CustomPostingList::insertSorted(PostingNode* newNode) {
    if (!m_head || newNode->docId < m_head->docId) {
        newNode->next = m_head;
        m_head = newNode;
        if (!m_tail) m_tail = m_head;
        ++m_docFreq;
        return;
    }

    PostingNode* current = m_head;
    if (current->docId == newNode->docId) {
        current->nameTermFreq += newNode->nameTermFreq;
        current->bodyTermFreq += newNode->bodyTermFreq;
        current->fieldMask |= newNode->fieldMask;
        delete newNode;
        return;
    }

    while (current->next && current->next->docId < newNode->docId) {
        current = current->next;
    }

    if (current->next && current->next->docId == newNode->docId) {
        current->next->nameTermFreq += newNode->nameTermFreq;
        current->next->bodyTermFreq += newNode->bodyTermFreq;
        current->next->fieldMask |= newNode->fieldMask;
        delete newNode;
        return;
    }

    newNode->next = current->next;
    current->next = newNode;
    if (!newNode->next) {
        m_tail = newNode;
    }
    ++m_docFreq;
}

CustomPostingList CustomPostingList::intersect(const CustomPostingList* a, const CustomPostingList* b) {
    CustomPostingList result;
    if (!a || !b || a->empty() || b->empty()) {
        return result;
    }

    const PostingNode* ptrA = a->m_head;
    const PostingNode* ptrB = b->m_head;

    while (ptrA && ptrB) {
        if (ptrA->docId == ptrB->docId) {
            uint16_t combName = ptrA->nameTermFreq + ptrB->nameTermFreq;
            uint16_t combBody = ptrA->bodyTermFreq + ptrB->bodyTermFreq;
            uint8_t  combMask = ptrA->fieldMask | ptrB->fieldMask;
            result.appendNode(ptrA->docId, combName, combBody, combMask);
            ptrA = ptrA->next;
            ptrB = ptrB->next;
        } else if (ptrA->docId < ptrB->docId) {
            ptrA = ptrA->next;
        } else {
            ptrB = ptrB->next;
        }
    }
    return result;
}

CustomPostingList CustomPostingList::unionLists(const CustomPostingList* a, const CustomPostingList* b) {
    CustomPostingList result;
    const PostingNode* ptrA = a ? a->m_head : nullptr;
    const PostingNode* ptrB = b ? b->m_head : nullptr;

    while (ptrA && ptrB) {
        if (ptrA->docId == ptrB->docId) {
            uint16_t combName = ptrA->nameTermFreq + ptrB->nameTermFreq;
            uint16_t combBody = ptrA->bodyTermFreq + ptrB->bodyTermFreq;
            uint8_t  combMask = ptrA->fieldMask | ptrB->fieldMask;
            result.appendNode(ptrA->docId, combName, combBody, combMask);
            ptrA = ptrA->next;
            ptrB = ptrB->next;
        } else if (ptrA->docId < ptrB->docId) {
            result.appendNode(ptrA->docId, ptrA->nameTermFreq, ptrA->bodyTermFreq, ptrA->fieldMask);
            ptrA = ptrA->next;
        } else {
            result.appendNode(ptrB->docId, ptrB->nameTermFreq, ptrB->bodyTermFreq, ptrB->fieldMask);
            ptrB = ptrB->next;
        }
    }

    while (ptrA) {
        result.appendNode(ptrA->docId, ptrA->nameTermFreq, ptrA->bodyTermFreq, ptrA->fieldMask);
        ptrA = ptrA->next;
    }

    while (ptrB) {
        result.appendNode(ptrB->docId, ptrB->nameTermFreq, ptrB->bodyTermFreq, ptrB->fieldMask);
        ptrB = ptrB->next;
    }

    return result;
}

CustomPostingList CustomPostingList::difference(const CustomPostingList* universe, const CustomPostingList* exclude) {
    CustomPostingList result;
    if (!universe || universe->empty()) {
        return result;
    }
    if (!exclude || exclude->empty()) {
        return *universe;
    }

    const PostingNode* ptrU = universe->m_head;
    const PostingNode* ptrE = exclude->m_head;

    while (ptrU && ptrE) {
        if (ptrU->docId == ptrE->docId) {
            ptrU = ptrU->next;
            ptrE = ptrE->next;
        } else if (ptrU->docId < ptrE->docId) {
            result.appendNode(ptrU->docId, ptrU->nameTermFreq, ptrU->bodyTermFreq, ptrU->fieldMask);
            ptrU = ptrU->next;
        } else {
            ptrE = ptrE->next;
        }
    }

    while (ptrU) {
        result.appendNode(ptrU->docId, ptrU->nameTermFreq, ptrU->bodyTermFreq, ptrU->fieldMask);
        ptrU = ptrU->next;
    }

    return result;
}

} // namespace sse::dsa
