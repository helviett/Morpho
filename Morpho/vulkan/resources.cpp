#include "resources.hpp"
#include "context.hpp"
#include <assert.h>

namespace Morpho::Vulkan {

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

RenderPassInfoBuilder& RenderPassInfoBuilder::layout(const RenderPassLayout& layout) {
    render_pass_info.layout = layout;
    return *this;
}

RenderPassInfoBuilder& RenderPassInfoBuilder::attachment(
    VkAttachmentLoadOp load_op,
    VkAttachmentStoreOp store_op,
    VkImageLayout final_layout
) {
    assert(render_pass_info.attachent_count < RenderPassInfo::max_attachment_count);
    render_pass_info.attachments[render_pass_info.attachent_count].load_op = load_op;
    render_pass_info.attachments[render_pass_info.attachent_count].store_op = store_op;
    render_pass_info.attachments[render_pass_info.attachent_count++].final_layout = final_layout;
    return *this;
}

RenderPassInfo RenderPassInfoBuilder::info() {
    return render_pass_info;
}

FramebufferInfoBuilder& FramebufferInfoBuilder::layout(const RenderPassLayout& layout) {
    framebuffer_info.layout = layout;
    return *this;
}
FramebufferInfoBuilder& FramebufferInfoBuilder::attachment(Texture image_view) {
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