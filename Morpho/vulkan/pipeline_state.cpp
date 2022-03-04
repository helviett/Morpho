#include "pipeline_state.hpp"

namespace Morpho::Vulkan {

PipelineState::PipelineState() {
    depth_stencil_state = {};
    depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
}

void PipelineState::add_shader(const Shader shader) {
    is_dirty = true;
    shaders[shader_count++] = shader;
}

uint32_t PipelineState::get_shader_count() const {
    return shader_count;
}

Shader PipelineState::get_shader(int index) const {
    return shaders[index];
}

void PipelineState::add_vertex_attribute_description(uint32_t binding, uint32_t location, VkFormat format, uint32_t offset) {
    is_dirty = true;
    VkVertexInputAttributeDescription desc;
    desc.binding = binding;
    desc.location = location;
    desc.format = format;
    desc.offset = offset;
    attribute_descriptions[attribute_description_count++] = desc;
}

void PipelineState::add_vertex_binding_description(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate) {
    is_dirty = true;
    binding_description.binding = 0;
    binding_description.stride = stride;
    binding_description.inputRate = input_rate;
}

VkVertexInputAttributeDescription PipelineState::get_vertex_attribute_description(uint32_t index) const {
    return attribute_descriptions[index];
}

VkVertexInputBindingDescription PipelineState::get_vertex_binding_description() const {
    return binding_description;
}

void PipelineState::set_pipeline_layout(PipelineLayout pipeline_layout) {
    is_dirty = true;
    this->pipeline_layout = pipeline_layout;
}

PipelineLayout PipelineState::get_pipeline_layout() const {
    return pipeline_layout;
}

bool PipelineState::get_is_dirty() const {
    return is_dirty;
}

bool PipelineState::get_and_clear_is_dirty() {
    auto old_is_dirty = is_dirty;
    is_dirty = false;
    return old_is_dirty;
}

uint32_t PipelineState::get_attribute_description_count() const {
    return attribute_description_count;
}

void PipelineState::set_depth_state(VkBool32 test_enable, VkBool32 write_enable, VkCompareOp compare_op) {
    is_dirty = true;
    depth_stencil_state.depthTestEnable = test_enable;
    depth_stencil_state.depthWriteEnable = write_enable;
    depth_stencil_state.depthCompareOp = compare_op;
}

VkPipelineDepthStencilStateCreateInfo PipelineState::get_depth_stencil_state() const {
    return depth_stencil_state;
}

}
