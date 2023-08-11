#include "resource_binding.hpp"

namespace Morpho::Vulkan {

ResourceBinding::ResourceBinding(): resouce_type(ResourceType::None) {}

ResourceBinding ResourceBinding::from_uniform_buffer(Buffer buffer, VkDeviceSize offset, VkDeviceSize range) {
    ResourceBinding binding;
    binding.resouce_type = ResourceType::UniformBuffer;
    binding.resource_info.buffer_info.buffer = buffer.buffer;
    binding.resource_info.buffer_info.offset = offset;
    binding.resource_info.buffer_info.range = range;
    return binding;
}

ResourceBinding ResourceBinding::from_combined_image_sampler(ImageView image_view, VkImageLayout image_layout, Sampler sampler) {
    ResourceBinding binding;
    binding.resouce_type = ResourceType::CombinedImageSampler;
    binding.resource_info.image_info.imageLayout = image_layout;
    binding.resource_info.image_info.imageView = image_view.image_view;
    binding.resource_info.image_info.sampler = sampler.sampler;
    return binding;
}

ResourceType ResourceBinding::get_resource_type() const {
    return resouce_type;
}

const VkDescriptorBufferInfo* ResourceBinding::get_buffer_info() const {
    return &resource_info.buffer_info;
}

const VkDescriptorImageInfo* ResourceBinding::get_image_info() const {
    return &resource_info.image_info;
}

}
