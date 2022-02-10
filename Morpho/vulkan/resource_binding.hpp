#pragma once
#include <vulkan/vulkan.h>
#include "buffer.hpp"

namespace Morpho::Vulkan {

enum class ResourceType {
    None,
    UniformBuffer,
};

// sort of discriminated union.
class ResourceBinding {
public:
    ResourceBinding();
    static ResourceBinding from_uniform_buffer(Buffer buffer, VkDeviceSize offset, VkDeviceSize range);

    ResourceType get_resource_type() const;
    const VkDescriptorBufferInfo* get_buffer_info() const;
private:
    VkDescriptorBufferInfo buffer_info;
    ResourceType resouce_type;
};

}