#pragma once
#include <vulkan/vulkan.h>

namespace Morpho::Vulkan {

class DescriptorSet {
public:
    DescriptorSet();
    DescriptorSet(VkDescriptorSet descriptor_set);

    VkDescriptorSet get_descriptor_set() const;
private:
    VkDescriptorSet descriptor_set;
};

}