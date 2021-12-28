#pragma once
#include <vulkan/vulkan.h>

namespace Morpho::Vulkan {
    class Framebuffer {
    public:
        Framebuffer(VkFramebuffer frambuffer);

        VkFramebuffer get_vulkan_handle();
    private:
        VkFramebuffer framebuffer;
    };
}
