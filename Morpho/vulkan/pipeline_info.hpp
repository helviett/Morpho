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
private:
    Shader shaders[2];
    uint32_t shader_count = 0;
};

}