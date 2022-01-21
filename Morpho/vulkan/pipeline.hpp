#pragma once
#include <vulkan/vulkan.h>

namespace Morpho::Vulkan {

class Pipeline {
public:
    Pipeline(VkPipeline pipeline);

    VkPipeline get_vulkan_handle() const;
private:
    VkPipeline handle;
};

}