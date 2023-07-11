#pragma once
#include <vulkan/vulkan.h>
#include "resources.hpp"

namespace Morpho::Vulkan {

class DescriptorPool {
public:
    DescriptorPool(VkDevice device, VkDescriptorPool pool, uint32_t max_sets);
    DescriptorPool();
    DescriptorPool(const DescriptorPool&) = delete;
    DescriptorPool& operator=(const DescriptorPool&) = delete;
    DescriptorPool(DescriptorPool&&) = default;
    DescriptorPool& operator=(DescriptorPool&&) = default;

    bool try_allocate_set(const DescriptorSetLayout& layout, DescriptorSet& set);
    void reset();
private:
    VkDevice device;
    VkDescriptorPool pool;
    uint32_t allocated;
    uint32_t max_sets;
};

}
