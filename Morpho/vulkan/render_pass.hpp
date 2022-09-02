#pragma once
#include <vulkan/vulkan.h>
#include "render_pass_layout.hpp"
#include <functional>
#include "../common/hash_utils.hpp"

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
    RenderPass();
    RenderPass(VkRenderPass render_pass, RenderPassLayout layout);

    VkRenderPass get_vulkan_handle() const;
    RenderPassLayout get_render_pass_layout() const;
private:
    VkRenderPass render_pass;
    RenderPassLayout layout;
};

}

template<>
struct std::hash<Morpho::Vulkan::RenderPassAttachmentInfo> {
    std::size_t operator()(Morpho::Vulkan::RenderPassAttachmentInfo const& info) const noexcept {
        std::size_t h = 0;
        Morpho::hash_combine(h, info.load_op, info.store_op, info.final_layout);
        return h;
    }
};

template<>
struct std::hash<Morpho::Vulkan::RenderPassInfo> {
    std::size_t operator()(Morpho::Vulkan::RenderPassInfo const& info) const noexcept {
        std::size_t h = 0;
        Morpho::hash_combine(h, info.max_attachment_count);
        for (std::size_t i = 0; i < info.attachent_count; i++) {
            Morpho::hash_combine(h, info.attachments[i]);
        }
        Morpho::hash_combine(h, info.layout.get_info());
        return h;
    }
};
