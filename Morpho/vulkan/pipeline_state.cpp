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
    VkVertexInputBindingDescription desc;
    desc.binding = binding;
    desc.stride = stride;
    desc.inputRate = input_rate;
    binding_descriptions[binding_description_count++] = desc;
}

VkVertexInputAttributeDescription PipelineState::get_vertex_attribute_description(uint32_t index) const {
    return attribute_descriptions[index];
}

VkVertexInputBindingDescription PipelineState::get_vertex_binding_description(uint32_t index) const {
    return binding_descriptions[index];
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

uint32_t PipelineState::get_vertex_binding_description_count() const {
    return binding_description_count;
}

void PipelineState::clear_vertex_attribute_descriptions() {
    is_dirty = true;
    attribute_description_count = 0;
}

void PipelineState::clear_vertex_binding_descriptions() {
    is_dirty = true;
    binding_description_count = 0;
}

void PipelineState::set_front_face(VkFrontFace front_face) {
    is_dirty = true;
    this->front_face = front_face;
}

VkFrontFace PipelineState::get_front_face() const {
    return front_face;
}

void PipelineState::set_cull_mode(VkCullModeFlags cull_mode) {
    is_dirty = true;
    this->cull_mode = cull_mode;
}

VkCullModeFlags PipelineState::get_cull_mode() const {
    return cull_mode;
}

void PipelineState::set_topology(VkPrimitiveTopology topology) {
    this->topology = topology;
}

VkPrimitiveTopology PipelineState::get_topology() const {
    return topology;
}

void PipelineState::enable_depth_bias(float depth_bias_constant_factor, float depth_bias_slope_factor) {
    depth_bias_enable = VK_TRUE;
    this->depth_bias_constant_factor = depth_bias_constant_factor;
    this->depth_bias_slope_factor = depth_bias_slope_factor;
}

void PipelineState::disable_depth_bias() {
    depth_bias_enable = VK_FALSE;
    depth_bias_constant_factor = 0.0f;
    depth_bias_slope_factor = 0.0f;
}

VkBool32 PipelineState::get_depth_bias_enable() const {
    return depth_bias_enable;
}

float PipelineState::get_depth_bias_constant_factor() const {
    return depth_bias_constant_factor;
}

float PipelineState::get_depth_bias_slope_factor() const {
    return depth_bias_slope_factor;
}

}
