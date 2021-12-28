#include "framebuffer.hpp"

namespace Morpho::Vulkan {

Framebuffer::Framebuffer(VkFramebuffer framebuffer) {
    this->framebuffer = framebuffer;
}

VkFramebuffer Framebuffer::get_vulkan_handle() {
    return framebuffer;
}

}
