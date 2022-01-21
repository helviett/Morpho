#include "pipeline.hpp"

namespace Morpho::Vulkan {

Pipeline::Pipeline(VkPipeline pipeline) {
    handle = pipeline;
}

VkPipeline Pipeline::get_vulkan_handle() const {
    return handle;
}

}