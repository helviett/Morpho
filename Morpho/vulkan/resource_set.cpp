#include "resource_set.hpp"

namespace Morpho::Vulkan {

ResourceSet::ResourceSet(): is_layout_dirty(true), contents_dirty(0) { }

void ResourceSet::set_uniform_buffer(uint32_t binding, Buffer buffer, VkDeviceSize offset, VkDeviceSize range) {
    bool is_resource_type_differ = bindings[binding].get_resource_type() != ResourceType::UniformBuffer;
    is_layout_dirty |= is_resource_type_differ;
    auto binding_dirty = is_resource_type_differ
        || buffer.buffer != bindings[binding].get_buffer_info()->buffer
        || offset != bindings[binding].get_buffer_info()->offset
        || range != bindings[binding].get_buffer_info()->range;
    contents_dirty |= (uint32_t)binding_dirty << binding;
    bindings[binding] = ResourceBinding::from_uniform_buffer(buffer, offset, range);
}

void ResourceSet::set_combined_image_sampler(
    uint32_t binding,
    Texture texture,
    Sampler sampler,
    VkImageLayout image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
) {
    bool is_resource_type_differ = bindings[binding].get_resource_type() != ResourceType::CombinedImageSampler;
    is_layout_dirty |= is_resource_type_differ;
    auto binding_dirty = is_resource_type_differ
        || texture.image_view != bindings[binding].get_image_info()->imageView
        || sampler.sampler != bindings[binding].get_image_info()->sampler
        || image_layout != bindings[binding].get_image_info()->imageLayout;
    bindings[binding] = ResourceBinding::from_combined_image_sampler(texture, image_layout, sampler);
}

const ResourceBinding& ResourceSet::get_binding(uint32_t binding) const {
    return bindings[binding];
}

bool ResourceSet::get_is_layout_dirty() const {
    return is_layout_dirty;
}

bool ResourceSet::get_is_contents_dirty() const {
    return contents_dirty;
}

void ResourceSet::clear_dirty_flags() {
    contents_dirty = 0;
    is_layout_dirty = false;
}

void ResourceSet::mark_all_dirty() {
    contents_dirty = ~0u;
    is_layout_dirty = true;
}

}
