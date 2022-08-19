#include "shader.hpp"
#include <stdexcept>

namespace Morpho::Vulkan {

Shader::Shader(): shader_module(VK_NULL_HANDLE), entry_point("") {}

Shader::Shader(const VkShaderModule shader_module): shader_module(shader_module) {}

Shader::Shader(const VkShaderModule shader_module, const ShaderStage stage): shader_module(shader_module), stage(stage), entry_point("") {}

Shader::Shader(const VkShaderModule shader_module, const ShaderStage stage, const std::string& entry_point):
    shader_module(shader_module), stage(stage), entry_point(entry_point) {}

void Shader::set_stage(const ShaderStage stage) {
    this->stage = stage;
}

void Shader::set_entry_point(const std::string& entry_point) {
    this->entry_point = entry_point;
}

VkShaderStageFlagBits Shader::shader_stage_to_vulkan(ShaderStage stage) {
    switch (stage)
    {
    case ShaderStage::NONE:
        throw std::runtime_error("Invalid shader stage.");
    case ShaderStage::VERTEX:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case ShaderStage::FRAGMENT:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    default:
        throw std::runtime_error("Not implemented");
    }
}

ShaderStage Shader::get_stage() const {
    return stage;
}

const std::string& Shader::get_entry_point() const {
    return entry_point;
}

VkShaderModule Shader::get_shader_module() const {
    return shader_module;
}

}
