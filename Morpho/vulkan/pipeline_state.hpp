#pragma once
#include <vulkan/vulkan.h>
#include "shader.hpp"
#include "pipeline.hpp"
#include "buffer.hpp"
#include "pipeline_layout.hpp"

namespace Morpho::Vulkan {

class PipelineState {
public:
    PipelineState();

    void add_shader(const Shader shader);
    uint32_t get_shader_count() const;
    Shader get_shader(int index) const;
    void add_vertex_attribute_description(uint32_t binding, uint32_t location, VkFormat format, uint32_t offset);
    uint32_t get_attribute_description_count() const;
    void add_vertex_binding_description(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate);
    VkVertexInputAttributeDescription get_vertex_attribute_description(uint32_t index) const;
    VkVertexInputBindingDescription get_vertex_binding_description() const;
    uint32_t get_occupied_descriptor_set_count() const;
    void set_pipeline_layout(PipelineLayout pipeline_layout);
    PipelineLayout get_pipeline_layout() const;
    bool get_is_dirty() const;
    bool get_and_clear_is_dirty();


    static const int MAX_VERTEX_ATTRIBUTE_DESCRIPTIONS = 10;
    static const int MAX_DESCRIPTOR_SETS = 4;
    static const int MAX_DESCRIPTOR_SET_BINDINGS = 16;
private:
    bool is_dirty = true;
    Shader shaders[2];
    uint32_t shader_count = 0;
    VkVertexInputAttributeDescription attribute_descriptions[MAX_VERTEX_ATTRIBUTE_DESCRIPTIONS];
    uint32_t attribute_description_count = 0;
    VkVertexInputBindingDescription binding_description;
    PipelineLayout pipeline_layout;
};

}

