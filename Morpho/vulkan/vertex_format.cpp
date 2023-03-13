#include "vertex_format.hpp"

namespace Morpho::Vulkan {

VertexFormat::VertexFormat() : hash(0) {}

VertexFormat::VertexFormat(std::size_t hash) : hash(hash) {}

std::size_t VertexFormat::get_hash() const {
    return hash;
}

}