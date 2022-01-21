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

}