#pragma once
#include <cstddef>

namespace Morpho {
    template<typename T, typename... Rest>
    inline void hash_combine(std::size_t& hash, const T& v, const Rest&... rest) {
        hash ^= std::hash<T>{}(v)  + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        (hash_combine(hash, rest), ...);
    }
}
