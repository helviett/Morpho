#include "command_buffer.hpp"

namespace Morpho::Vulkan {

VkCommandBuffer CommandBuffer::get_vulkan_handle() const {
    return command_buffer;
}

CommandBuffer::CommandBuffer(VkCommandBuffer command_buffer, Context* context) {
    this->command_buffer = command_buffer;
    this->context = context;
}

}
