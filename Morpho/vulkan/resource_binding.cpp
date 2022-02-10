#include "resource_binding.hpp"

namespace Morpho::Vulkan {

ResourceBinding::ResourceBinding(): resouce_type(ResourceType::None) {}

ResourceBinding ResourceBinding::from_uniform_buffer(Buffer buffer, VkDeviceSize offset, VkDeviceSize range) {
    ResourceBinding binding;
    binding.resouce_type = ResourceType::UniformBuffer;
    binding.buffer_info.buffer = buffer.get_buffer();
    binding.buffer_info.offset = offset;
    binding.buffer_info.range = range;
    return binding;
}

ResourceType ResourceBinding::get_resource_type() const {
    return resouce_type;
}

const VkDescriptorBufferInfo* ResourceBinding::get_buffer_info() const {
    return &buffer_info;
}

}