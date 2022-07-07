#pragma once
#include <vulkan/vulkan.h>
#include "render_pass_layout.hpp"
#include "image_view.hpp"

namespace Morpho::Vulkan {

struct FramebufferInfo {
    static constexpr uint32_t max_attachment_count = 8 + 1;
    RenderPassLayout layout;
    uint32_t attachment_count = 0;
    ImageView attachments[max_attachment_count];
    VkExtent2D extent;
};

class FramebufferInfoBuilder {
public:
    FramebufferInfoBuilder& layout(const RenderPassLayout& layout);
    FramebufferInfoBuilder& attachment(ImageView image_view);
    FramebufferInfoBuilder& extent(VkExtent2D extent);
    FramebufferInfo info();
private:
    FramebufferInfo framebuffer_info;
};

class Framebuffer {
public:
    Framebuffer(VkFramebuffer frambuffer);

    VkFramebuffer get_vulkan_handle();
private:
    VkFramebuffer framebuffer;
};

}
