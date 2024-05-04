#pragma once
#include <stdint.h>

namespace Morpho {
    inline uint64_t round_up(uint64_t size, uint64_t alignment) {
        return (size + alignment - 1) & ~(alignment - 1);
    }

    template<typename T>
    inline T max(T lhs, T rhs) {
        return lhs < rhs ? rhs : lhs;
    }

    template<typename T>
    inline T min(T lhs, T rhs) {
        return lhs > rhs ? rhs : lhs;
    }
}
