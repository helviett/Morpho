#pragma once
#include <vulkan/vulkan.h>
#include "shader.hpp"
#include "pipeline.hpp"

namespace Morpho::Vulkan {

class PipelineInfo {
public:

    PipelineInfo();
    PipelineInfo(Shader vertex, Shader fragment);

    void add_shader(const Shader shader);
    uint32_t get_shader_count() const;
    Shader get_shader(int index) const;
    void add_vertex_attribute_description(uint32_t binding, uint32_t location, VkFormat format, uint32_t offset);
    void add_vertex_binding_description(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate);
    VkVertexInputAttributeDescription get_vertex_attribute_description(uint32_t index) const;
    VkVertexInputBindingDescription get_vertex_binding_description() const;
    uint32_t get_attribute_description_count() const;


    static const int MAXIMUM_VERTEX_ATTRIBUTE_DESCRIPTION_COUNT = 10;
private:
    Shader shaders[2];
    uint32_t shader_count = 0;
    VkVertexInputAttributeDescription attribute_descriptions[MAXIMUM_VERTEX_ATTRIBUTE_DESCRIPTION_COUNT];
    uint32_t attribute_description_count = 0;
    VkVertexInputBindingDescription binding_description;
};

}