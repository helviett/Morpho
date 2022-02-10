#include "descriptor_set_layout.hpp"

namespace Morpho::Vulkan {

DescriptorSetLayout::DescriptorSetLayout(): descriptor_set_layout(VK_NULL_HANDLE) { }

DescriptorSetLayout::DescriptorSetLayout(VkDescriptorSetLayout descriptor_set_layout)
    : descriptor_set_layout(descriptor_set_layout) { }

VkDescriptorSetLayout DescriptorSetLayout::get_descriptor_set_layout() const {
    return descriptor_set_layout;
}

}