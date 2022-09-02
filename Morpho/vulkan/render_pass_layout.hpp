#pragma once
#include <vulkan/vulkan.h>
#include <optional>
#include "../common/hash_utils.hpp"

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

template<>
struct std::hash<Morpho::Vulkan::RenderPassLayoutAttachmentInfo> {
    std::size_t operator()(Morpho::Vulkan::RenderPassLayoutAttachmentInfo const& info) const noexcept {
        std::size_t h = 0;
        Morpho::hash_combine(h, info.format);
        return h;
    }
};

template<>
struct std::hash<Morpho::Vulkan::RenderPassLayoutInfo> {
    std::size_t operator()(Morpho::Vulkan::RenderPassLayoutInfo const& info) const noexcept {
        std::size_t h = 0;
        Morpho::hash_combine(h, info.max_attachment_count);
        for (std::size_t i = 0; i < info.attachent_count; i++) {
            Morpho::hash_combine(h, info.attachments[i]);
        }
        return h;
    }
};
