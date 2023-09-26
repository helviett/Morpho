#pragma once
#include <stdint.h>

namespace Morpho {
    inline uint64_t round_up_to_alignment(uint64_t size, uint64_t alignment) {
        return (size + alignment - 1) & ~(alignment - 1);
    }
}
