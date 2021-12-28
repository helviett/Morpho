#pragma once
#include <vulkan/vulkan.h>

namespace Morpho::Vulkan {
    struct RenderPassInfo {
        // Keep vulkan types for now.
        // Assume image_view is swapchain image view.
        VkImageView image_view;
        VkClearValue clear_value;
    };
}
