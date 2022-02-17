#include "resource_binding.hpp"

namespace Morpho::Vulkan {

ResourceBinding::ResourceBinding(): resouce_type(ResourceType::None) {}

ResourceBinding ResourceBinding::from_uniform_buffer(Buffer buffer, VkDeviceSize offset, VkDeviceSize range) {
    ResourceBinding binding;
    binding.resouce_type = ResourceType::UniformBuffer;
    binding.resource_info.buffer_info.buffer = buffer.get_buffer();
    binding.resource_info.buffer_info.offset = offset;
    binding.resource_info.buffer_info.range = range;
    return binding;
}

ResourceBinding ResourceBinding::from_combined_image_sampler(ImageView image_view, Sampler sampler) {
    ResourceBinding binding;
    binding.resouce_type = ResourceType::CombinedImageSampler;
    binding.resource_info.image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    binding.resource_info.image_info.imageView = image_view.get_image_view();
    binding.resource_info.image_info.sampler = sampler.get_sampler();
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
