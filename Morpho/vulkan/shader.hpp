#pragma once
#include <vulkan/vulkan.h>
#include <string>

namespace Morpho::Vulkan {

enum class ShaderStage {
    None = 0,
    Vertex = 1,
    Fragment = 2,
};

class Shader {
public:
    Shader();
    Shader(const VkShaderModule module);
    Shader(const VkShaderModule module, const ShaderStage stage);
    Shader(const VkShaderModule module, const ShaderStage stage, const std::string& entry_point);

    void set_stage(const ShaderStage stage);
    void set_entry_point(const std::string& entry_point);
    ShaderStage get_stage() const;
    const std::string& get_entry_point() const;
    VkShaderModule get_shader_module() const;

    static VkShaderStageFlagBits shader_stage_to_vulkan(ShaderStage stage);
private:
    VkShaderModule shader_module;
    ShaderStage stage;
    std::string entry_point;
};

}