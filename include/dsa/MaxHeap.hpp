#pragma once
#include "CustomMaxHeap.hpp"

namespace sse {
    template <typename T, typename Compare = std::less<T>>
    using MaxHeap = sse::dsa::CustomMaxHeap<T, Compare>;
}
