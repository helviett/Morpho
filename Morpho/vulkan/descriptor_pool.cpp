#include "descriptor_pool.hpp"

namespace Morpho::Vulkan {

DescriptorPool::DescriptorPool(VkDevice device, VkDescriptorPool pool, uint32_t max_sets)
    : device(device), pool(pool), max_sets(max_sets), allocated(0) { }

DescriptorPool::DescriptorPool()
    : device(VK_NULL_HANDLE), pool(VK_NULL_HANDLE), max_sets(0), allocated(0) { }

bool DescriptorPool::try_allocate_set(const VkDescriptorSetLayout& layout, DescriptorSet& set) {
    if (allocated >= max_sets) {
        return false;
    }
    VkDescriptorSetLayout set_layout = layout;
    VkDescriptorSetAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &set_layout;
    VkDescriptorSet descriptor_set;
    vkAllocateDescriptorSets(device, &info, &descriptor_set);
    set = DescriptorSet(descriptor_set);
    allocated++;
    return true;
}

void DescriptorPool::reset() {
    allocated = 0;
    vkResetDescriptorPool(device, pool, 0);
}

}
