#pragma once
#include "image_view.hpp"

namespace Morpho::Vulkan {
    struct RenderPassInfo {
        // Assume image_view is swapchain image view.
        ImageView color_attachment_image_view;
        VkClearValue color_attachment_clear_value;
        ImageView depth_attachment_image_view;
        VkClearValue depth_attachment_clear_value;
    };
}
