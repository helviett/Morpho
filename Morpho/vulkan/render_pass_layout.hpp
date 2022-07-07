#pragma once
#include <vulkan/vulkan.h>
#include <optional>

namespace Morpho::Vulkan {

struct SubpassInfo {
    static constexpr uint32_t max_color_attachment_count = 8;
    uint32_t color_attachments[max_color_attachment_count];
    uint32_t color_attachment_count = 0;
    std::optional<uint32_t> depth_attachment = std::nullopt;
    // uint32_t input_attachments[16];
    // uint32_t input_attachment_count = 0;
};

struct RenderPassLayoutAttachmentInfo {
    VkFormat format;
};

struct RenderPassLayoutInfo {
    static constexpr uint32_t max_attachment_count = 16;
    RenderPassLayoutAttachmentInfo attachments[max_attachment_count];
    uint32_t attachent_count = 0;
    SubpassInfo subpass;
};

class RenderPassLayoutInfoBuilder {
public:
    RenderPassLayoutInfoBuilder& attachment(VkFormat format);
    RenderPassLayoutInfoBuilder& subpass(
        std::initializer_list<uint32_t> color_attachments,
        std::optional<uint32_t> depth_attachment
    );
    // Should it return referece?
    RenderPassLayoutInfo info();
private:
    RenderPassLayoutInfo layout_info{};
};

class RenderPassLayout {
public:
    RenderPassLayout();
    RenderPassLayout(RenderPassLayoutInfo info);
    RenderPassLayout(RenderPassLayoutInfo info, VkRenderPass render_pass);

    VkRenderPass get_vulkan_handle() const;
    const RenderPassLayoutInfo& get_info() const;
private:
    VkRenderPass render_pass;
    RenderPassLayoutInfo info;
};


}
