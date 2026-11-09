#include "indexing/InvertedIndex.hpp"

namespace sse::indexing {

InvertedIndex::InvertedIndex() = default;

InvertedIndex::~InvertedIndex() noexcept {
    deallocateAllLists();
}

InvertedIndex::InvertedIndex(InvertedIndex&& other) noexcept {
    std::unique_lock<std::shared_mutex> lock(other.m_mutex);
    m_table = std::move(other.m_table);
    m_trie = std::move(other.m_trie);
    m_lengthStats = std::move(other.m_lengthStats);
    m_totalPostings = other.m_totalPostings;
    other.m_totalPostings = 0;
}

InvertedIndex& InvertedIndex::operator=(InvertedIndex&& other) noexcept {
    if (this != &other) {
        std::unique_lock<std::shared_mutex> lockThis(m_mutex, std::defer_lock);
        std::unique_lock<std::shared_mutex> lockOther(other.m_mutex, std::defer_lock);
        std::lock(lockThis, lockOther);

        deallocateAllLists();
        m_table = std::move(other.m_table);
        m_trie = std::move(other.m_trie);
        m_lengthStats = std::move(other.m_lengthStats);
        m_totalPostings = other.m_totalPostings;
        other.m_totalPostings = 0;
    }
    return *this;
}

void InvertedIndex::deallocateAllLists() noexcept {
    auto** buckets = m_table.getBuckets();
    std::size_t numBuckets = m_table.bucketCount();
    for (std::size_t i = 0; i < numBuckets; ++i) {
        auto* entry = buckets[i];
        while (entry) {
            delete entry->value;
            entry->value = nullptr;
            entry = entry->next;
        }
    }
}

void InvertedIndex::addTerm(const std::string& term,
                            DocID docId,
                            uint16_t nameFreq,
                            uint16_t bodyFreq,
                            uint8_t fieldMask) {
    if (term.empty() || docId == INVALID_DOC_ID) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);

    dsa::CustomPostingList** listPtr = m_table.find(term);
    dsa::CustomPostingList* list = nullptr;

    if (!listPtr || !(*listPtr)) {
        list = new dsa::CustomPostingList();
        m_table.insert(term, list);
        if (dsa::CustomTrie::isValidAsciiWord(term)) {
            m_trie.insert(term, 1);
        }
    } else {
        list = *listPtr;
    }

    uint32_t prevFreq = list->documentFrequency();
    list->appendNode(docId, nameFreq, bodyFreq, fieldMask);
    if (list->documentFrequency() > prevFreq) {
        ++m_totalPostings;
    }
    m_lengthStats.accumulateDocumentLength(docId, nameFreq, bodyFreq);
}

bool InvertedIndex::removeDocument(DocID docId) {
    if (docId == INVALID_DOC_ID) return false;

    std::unique_lock<std::shared_mutex> lock(m_mutex);

    bool anyRemoved = false;
    dsa::CustomDynamicArray<std::string> emptyTerms;

    auto** buckets = m_table.getBuckets();
    std::size_t numBuckets = m_table.bucketCount();

    for (std::size_t i = 0; i < numBuckets; ++i) {
        auto* entry = buckets[i];
        while (entry) {
            dsa::CustomPostingList* list = entry->value;
            if (list && list->removeDocId(docId)) {
                anyRemoved = true;
                if (m_totalPostings > 0) {
                    --m_totalPostings;
                }
                if (list->empty()) {
                    emptyTerms.push_back(entry->key);
                }
            }
            entry = entry->next;
        }
    }

    // Clean up empty terms
    for (std::size_t k = 0; k < emptyTerms.size(); ++k) {
        const std::string& term = emptyTerms[k];
        dsa::CustomPostingList** listPtr = m_table.find(term);
        if (listPtr && *listPtr) {
            delete *listPtr;
            *listPtr = nullptr;
        }
        m_table.remove(term);
        if (dsa::CustomTrie::isValidAsciiWord(term)) {
            m_trie.remove(term);
        }
    }

    m_lengthStats.removeDocument(docId);

    return anyRemoved;
}

bool InvertedIndex::removeTerm(const std::string& term) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    dsa::CustomPostingList** listPtr = m_table.find(term);
    if (!listPtr || !(*listPtr)) {
        return false;
    }
    dsa::CustomPostingList* list = *listPtr;
    uint32_t df = list->documentFrequency();
    if (m_totalPostings >= df) {
        m_totalPostings -= df;
    } else {
        m_totalPostings = 0;
    }
    delete list;
    m_table.remove(term);
    if (dsa::CustomTrie::isValidAsciiWord(term)) {
        m_trie.remove(term);
    }
    return true;
}

void InvertedIndex::clear() noexcept {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    deallocateAllLists();
    m_table.clear();
    m_trie.clear();
    m_lengthStats.clear();
    m_totalPostings = 0;
}

const dsa::CustomPostingList* InvertedIndex::getPostingList(const std::string& term) const noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    const dsa::CustomPostingList* const* listPtr = m_table.find(term);
    return (listPtr && *listPtr) ? *listPtr : nullptr;
}

dsa::CustomPostingList* InvertedIndex::getMutablePostingList(const std::string& term) noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    dsa::CustomPostingList** listPtr = m_table.find(term);
    return (listPtr && *listPtr) ? *listPtr : nullptr;
}

uint32_t InvertedIndex::getDocumentFrequency(const std::string& term) const noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    const dsa::CustomPostingList* const* listPtr = m_table.find(term);
    if (listPtr && *listPtr) {
        return (*listPtr)->documentFrequency();
    }
    return 0;
}

bool InvertedIndex::containsTerm(const std::string& term) const noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_table.contains(term);
}

std::size_t InvertedIndex::vocabularySize() const noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_table.size();
}

std::size_t InvertedIndex::trieVocabularySize() const noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_trie.wordCount();
}

uint64_t InvertedIndex::totalPostingsCount() const noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_totalPostings;
}

void InvertedIndex::forEachTerm(const std::function<void(const std::string& term, const dsa::CustomPostingList& list)>& callback) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    const auto* const* buckets = m_table.getBuckets();
    std::size_t numBuckets = m_table.bucketCount();

    for (std::size_t i = 0; i < numBuckets; ++i) {
        const auto* entry = buckets[i];
        while (entry) {
            if (entry->value) {
                callback(entry->key, *entry->value);
            }
            entry = entry->next;
        }
    }
}

} // namespace sse::indexing
