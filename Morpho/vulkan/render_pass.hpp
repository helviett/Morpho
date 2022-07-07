#pragma once
#include <vulkan/vulkan.h>
#include "render_pass_layout.hpp"

namespace Morpho::Vulkan {

struct RenderPassAttachmentInfo {
    VkAttachmentLoadOp load_op;
    VkAttachmentStoreOp store_op;
    VkImageLayout final_layout;
};

struct RenderPassInfo {
    static constexpr uint32_t max_attachment_count = 16;
    RenderPassAttachmentInfo attachments[max_attachment_count];
    uint32_t attachent_count = 0;
    RenderPassLayout layout;
};

class RenderPassInfoBuilder {
public:
    RenderPassInfoBuilder& layout(const RenderPassLayout& layout);
    RenderPassInfoBuilder& attachment(
        VkAttachmentLoadOp load_op,
        VkAttachmentStoreOp store_op,
        VkImageLayout final_layout
    );
    RenderPassInfo info();
private:
    RenderPassInfo render_pass_info;
};


class RenderPass {
public:
    RenderPass(VkRenderPass render_pass);

    VkRenderPass get_vulkan_handle() const;
private:
    VkRenderPass render_pass;
};

}
