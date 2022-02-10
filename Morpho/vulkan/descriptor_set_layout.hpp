#pragma once
#include <vulkan/vulkan.h>

namespace Morpho::Vulkan {

class DescriptorSetLayout {
public:
    DescriptorSetLayout();
    DescriptorSetLayout(VkDescriptorSetLayout descriptor_set_layout);

    VkDescriptorSetLayout get_descriptor_set_layout() const;
private:
    VkDescriptorSetLayout descriptor_set_layout;
};

}