#pragma once
#include <stdint.h>

namespace Morpho {
    inline uint64_t align_up_pow2(uint64_t size, uint64_t alignment) {
        return (size + alignment - 1) & ~(alignment - 1);
    }

    inline uint64_t align_up(uint64_t size, uint64_t alignment) {
        return ((size + alignment - 1) / alignment) * alignment;
    }

    inline uint64_t align_down_pow2(uint64_t size, uint64_t alignment) {
        return size & ~(alignment - 1);
    }

    inline uint64_t align_down(uint64_t size, uint64_t alignment) {
        return (size / alignment) * alignment;
    }

    inline bool is_pow2(uint64_t value) {
        return (value & (value - 1)) == 0;
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
