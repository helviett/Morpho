#include "descriptor_set.hpp"

namespace Morpho::Vulkan {

DescriptorSet::DescriptorSet(): descriptor_set(VK_NULL_HANDLE) { }

DescriptorSet::DescriptorSet(VkDescriptorSet descriptor_set): descriptor_set(descriptor_set) { }

VkDescriptorSet DescriptorSet::get_descriptor_set() const {
    return descriptor_set;
}

}