#include "pipeline.hpp"

namespace Morpho::Vulkan {

Pipeline::Pipeline(): pipeline(VK_NULL_HANDLE) { }

Pipeline::Pipeline(VkPipeline pipeline) {
    this->pipeline = pipeline;
}

VkPipeline Pipeline::get_pipeline() const {
    return pipeline;
}

}