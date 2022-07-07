#include "render_pass_layout.hpp"
#include <cassert>

namespace Morpho::Vulkan {

RenderPassLayout::RenderPassLayout()
    : info{}, render_pass(VK_NULL_HANDLE) { }

RenderPassLayout::RenderPassLayout(RenderPassLayoutInfo info, VkRenderPass render_pass)
    : info(info), render_pass(render_pass) { }

RenderPassLayout::RenderPassLayout(RenderPassLayoutInfo info)
    : info(info), render_pass(VK_NULL_HANDLE) { }

VkRenderPass RenderPassLayout::get_vulkan_handle() const {
    return render_pass;
}

const RenderPassLayoutInfo& RenderPassLayout::get_info() const {
    return info;
}

RenderPassLayoutInfoBuilder& RenderPassLayoutInfoBuilder::attachment(VkFormat format) {
    layout_info.attachments[layout_info.attachent_count++].format = format;
    assert(layout_info.attachent_count <= RenderPassLayoutInfo::max_attachment_count);
    return *this;
}

RenderPassLayoutInfoBuilder& RenderPassLayoutInfoBuilder::subpass(
    std::initializer_list<uint32_t> color_attachments,
    std::optional<uint32_t> depth_attachment
) {
    assert(color_attachments.size() <= SubpassInfo::max_color_attachment_count);
    layout_info.subpass.color_attachment_count = (uint32_t)color_attachments.size();
    std::copy(color_attachments.begin(), color_attachments.end(), layout_info.subpass.color_attachments);
    layout_info.subpass.depth_attachment = depth_attachment;
    return *this;
}

RenderPassLayoutInfo RenderPassLayoutInfoBuilder::info() {
    return layout_info;
}

}
