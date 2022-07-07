#include "framebuffer.hpp"
#include <cassert>

namespace Morpho::Vulkan {

Framebuffer::Framebuffer(VkFramebuffer framebuffer) {
    this->framebuffer = framebuffer;
}

VkFramebuffer Framebuffer::get_vulkan_handle() {
    return framebuffer;
}

FramebufferInfoBuilder& FramebufferInfoBuilder::layout(const RenderPassLayout& layout) {
    framebuffer_info.layout = layout;
    return *this;
}
FramebufferInfoBuilder& FramebufferInfoBuilder::attachment(ImageView image_view) {
    assert(framebuffer_info.attachment_count < FramebufferInfo::max_attachment_count);
    framebuffer_info.attachments[framebuffer_info.attachment_count++] = image_view;
    return *this;
}
FramebufferInfoBuilder& FramebufferInfoBuilder::extent(VkExtent2D extent) {
    framebuffer_info.extent = extent;
    return *this;
}
FramebufferInfo FramebufferInfoBuilder::info() {
    return framebuffer_info;
}

}
