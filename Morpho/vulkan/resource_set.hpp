#pragma once
#include <vulkan/vulkan.h>
#include "resource_binding.hpp"

namespace Morpho::Vulkan {

class ResourceSet {
public:
    ResourceSet();

    void set_uniform_buffer(uint32_t binding, Buffer buffer, VkDeviceSize offset, VkDeviceSize range);
    const ResourceBinding& get_binding(uint32_t binding) const;
    bool get_is_layout_dirty() const;
    bool get_is_contents_dirty() const;
    void clear_dirty_flags();
    void mark_all_dirty();
private:
    ResourceBinding bindings[16];
    bool is_layout_dirty;
    uint32_t contents_dirty;
};

}