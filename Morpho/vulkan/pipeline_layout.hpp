#pragma once
#include <vulkan/vulkan.h>
#include "descriptor_set_layout.hpp"

namespace Morpho::Vulkan {

class PipelineLayout {
public:
    PipelineLayout();
    PipelineLayout(VkPipelineLayout pipeline_layout, DescriptorSetLayout descriptor_set_layouts[4]);

    VkPipelineLayout get_pipeline_layout() const;
    DescriptorSetLayout get_descriptor_set_layout(uint32_t index) const;
private:
    VkPipelineLayout pipeline_layout;
    DescriptorSetLayout descriptor_set_layouts[4];
};

}