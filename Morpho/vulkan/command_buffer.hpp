#pragma once
#include <vulkan/vulkan.h>

namespace Morpho::Vulkan {
    class Context;

    class CommandBuffer {
    public:
        CommandBuffer(VkCommandBuffer command_buffer, Context* context);
        VkCommandBuffer get_vulkan_handle() const;
    private:
        VkCommandBuffer command_buffer;
        Context* context;
    };
}
