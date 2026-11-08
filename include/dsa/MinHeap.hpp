#pragma once
#include "CustomMinHeap.hpp"

namespace sse {
    template <typename T, typename Compare = std::greater<T>>
    using MinHeap = sse::dsa::CustomMinHeap<T, Compare>;
}
