#pragma once
#include <vulkan/vulkan.h>
#include "resource_binding.hpp"
#include "limits.hpp"
#include "sampler.hpp"
#include "image_view.hpp"

namespace Morpho::Vulkan {

class ResourceSet {
public:
    ResourceSet();

    void set_uniform_buffer(uint32_t binding, Buffer buffer, VkDeviceSize offset, VkDeviceSize range);
    void set_combined_image_sampler(
        uint32_t binding,
        ImageView image_view,
        Sampler sampler,
        VkImageLayout image_layout
    );
    const ResourceBinding& get_binding(uint32_t binding) const;
    bool get_is_layout_dirty() const;
    bool get_is_contents_dirty() const;
    void clear_dirty_flags();
    void mark_all_dirty();
private:
    ResourceBinding bindings[Limits::MAX_DESCRIPTOR_SET_BINDING_COUNT];
    bool is_layout_dirty;
    uint32_t contents_dirty;
};

}
