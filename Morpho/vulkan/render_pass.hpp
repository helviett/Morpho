#pragma once
#include <vulkan/vulkan.h>

namespace Morpho::Vulkan {
    class RenderPass {
    public:
        RenderPass(VkRenderPass render_pass);

        VkRenderPass get_vulkan_handle();
    private:
        VkRenderPass render_pass;
    };
}
