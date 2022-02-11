#include "pipeline_layout.hpp"

namespace Morpho::Vulkan {

PipelineLayout::PipelineLayout(): pipeline_layout(VK_NULL_HANDLE) { }

PipelineLayout::PipelineLayout(
    VkPipelineLayout pipeline_layout,
    DescriptorSetLayout descriptor_set_layouts[Limits::MAX_DESCRIPTOR_SET_COUNT]
): pipeline_layout(pipeline_layout) {
    for (uint32_t i = 0; i < Limits::MAX_DESCRIPTOR_SET_COUNT; i++) {
        this->descriptor_set_layouts[i] = descriptor_set_layouts[i];
    }
}

VkPipelineLayout PipelineLayout::get_pipeline_layout() const {
    return pipeline_layout;
}

DescriptorSetLayout PipelineLayout::get_descriptor_set_layout(uint32_t index) const {
    return descriptor_set_layouts[index];
}

}
