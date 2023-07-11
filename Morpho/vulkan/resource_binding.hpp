#pragma once
#include <vulkan/vulkan.h>
#include "resources.hpp"

namespace Morpho::Vulkan {

enum class ResourceType {
    None,
    UniformBuffer,
    CombinedImageSampler,
};

// sort of discriminated union.
class ResourceBinding {
public:
    ResourceBinding();
    static ResourceBinding from_uniform_buffer(Buffer buffer, VkDeviceSize offset, VkDeviceSize range);
    static ResourceBinding from_combined_image_sampler(ImageView image_view, VkImageLayout image_layout, Sampler sampler);

    ResourceType get_resource_type() const;
    const VkDescriptorBufferInfo* get_buffer_info() const;
    const VkDescriptorImageInfo* get_image_info() const;
private:
    union {
        VkDescriptorBufferInfo buffer_info;
        VkDescriptorImageInfo image_info;
    } resource_info;

    ResourceType resouce_type;
};

}
