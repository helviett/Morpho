#include "render_pass.hpp"
#include <cassert>

namespace Morpho::Vulkan {

RenderPass::RenderPass(VkRenderPass render_pass) {
    this->render_pass = render_pass;
}

VkRenderPass RenderPass::get_vulkan_handle() const {
    return render_pass;
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

}
