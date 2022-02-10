#pragma once
#include <vulkan/vulkan.h>

namespace Morpho::Vulkan {

class Pipeline {
public:
    Pipeline();
    Pipeline(VkPipeline pipeline);

    VkPipeline get_pipeline() const;
private:
    VkPipeline pipeline;
};

}