#pragma once
#include <cstddef>

namespace Morpho::Vulkan {

class VertexFormat {
public:
    VertexFormat();
    VertexFormat(std::size_t hash);

    std::size_t get_hash() const;
private:
    std::size_t hash;
};

}