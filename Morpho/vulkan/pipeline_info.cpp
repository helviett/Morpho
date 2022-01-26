#include "pipeline_info.hpp"

namespace Morpho::Vulkan {

PipelineInfo::PipelineInfo() {}

PipelineInfo::PipelineInfo(Shader vertex, Shader fragment) {
    add_shader(vertex);
    add_shader(fragment);
}

void PipelineInfo::add_shader(const Shader shader) {
    shaders[shader_count++] = shader;
}

uint32_t PipelineInfo::get_shader_count() const {
    return shader_count;
}

Shader PipelineInfo::get_shader(int index) const {
    return shaders[index];
}

void PipelineInfo::add_vertex_attribute_description(uint32_t binding, uint32_t location, VkFormat format, uint32_t offset) {
    VkVertexInputAttributeDescription desc;
    desc.binding = binding;
    desc.location = location;
    desc.format = format;
    desc.offset = offset;
    attribute_descriptions[attribute_description_count++] = desc;
}

void PipelineInfo::add_vertex_binding_description(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate) {
    binding_description.binding = 0;
    binding_description.stride = stride;
    binding_description.inputRate = input_rate;
}

VkVertexInputAttributeDescription PipelineInfo::get_vertex_attribute_description(uint32_t index) const {
    return attribute_descriptions[index];
}

VkVertexInputBindingDescription PipelineInfo::get_vertex_binding_description() const {
    return binding_description;
}

uint32_t PipelineInfo::get_attribute_description_count() const {
    return attribute_description_count;
}

}