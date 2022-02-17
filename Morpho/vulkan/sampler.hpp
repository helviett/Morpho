#pragma once
#include <vulkan/vulkan.h>

namespace Morpho::Vulkan {

class Sampler {
public:
    Sampler();
    Sampler(VkSampler sampler);

    VkSampler get_sampler() const;
private:
    VkSampler sampler;
};

}
