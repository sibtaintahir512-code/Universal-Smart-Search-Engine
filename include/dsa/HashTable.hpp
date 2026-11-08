#pragma once
#include "dsa/CustomHashTable.hpp"

namespace sse::dsa {
    template <typename K, typename V, typename Hash = Fnv1aHash>
    using HashTable = CustomHashTable<K, V, Hash>;
}
