#include "sampler.hpp"

namespace Morpho::Vulkan {

Sampler::Sampler(): sampler(VK_NULL_HANDLE) { }

Sampler::Sampler(VkSampler sampler): sampler(sampler) { }

VkSampler Sampler::get_sampler() const {
    return sampler;
}

}
