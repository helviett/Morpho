#include "resources.hpp"
#include "context.hpp"
#include <assert.h>

namespace Morpho::Vulkan {

Buffer::Buffer(): context(nullptr), buffer(VK_NULL_HANDLE), allocation(VK_NULL_HANDLE), allocation_info() {}

Buffer::Buffer(Context* context, VkBuffer buffer, VmaAllocation allocation, VmaAllocationInfo allocation_info)
    : context(context), buffer(buffer), allocation(allocation), allocation_info(allocation_info) { }

void Buffer::update(const void* data, VkDeviceSize size) {
    void* mapped;
    context->map_memory(allocation, &mapped);
    memcpy(mapped, data, size);
    context->unmap_memory(allocation);
}

VkBuffer Buffer::get_buffer() const {
    return buffer;
}

VmaAllocation Buffer::get_allocation() const {
    return allocation;
}

DescriptorSet::DescriptorSet(): descriptor_set(VK_NULL_HANDLE) { }

DescriptorSet::DescriptorSet(VkDescriptorSet descriptor_set): descriptor_set(descriptor_set) { }

VkDescriptorSet DescriptorSet::get_descriptor_set() const {
    return descriptor_set;
}

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

RenderPass::RenderPass() : render_pass(VK_NULL_HANDLE), layout{} {

}

RenderPass::RenderPass(VkRenderPass render_pass, RenderPassLayout layout) : render_pass(render_pass), layout(layout) { }

VkRenderPass RenderPass::get_vulkan_handle() const {
    return render_pass;
}

RenderPassLayout RenderPass::get_render_pass_layout() const {
    return layout;
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

Image::Image(): image(VK_NULL_HANDLE), allocation(VK_NULL_HANDLE), allocation_info({}) { }

Image::Image(VkImage image, VmaAllocation allocation, VmaAllocationInfo allocation_info)
    : image(image), allocation(allocation), allocation_info(allocation_info) { }

Image::Image(VkImage image): image(image), allocation(VK_NULL_HANDLE),allocation_info({}) { }

VkImage Image::get_image() const {
    return image;
}

VmaAllocation Image::get_allocation() const {
    return allocation;
}

ImageView::ImageView(): image_view(VK_NULL_HANDLE) { }

ImageView::ImageView(VkImageView image_view, Image image): image_view(image_view), image(image) { }

VkImageView ImageView::get_image_view() const {
    return image_view;
}

Sampler::Sampler(): sampler(VK_NULL_HANDLE) { }

Sampler::Sampler(VkSampler sampler): sampler(sampler) { }

VkSampler Sampler::get_sampler() const {
    return sampler;
}

Shader::Shader(): shader_module(VK_NULL_HANDLE), entry_point("") {}

Shader::Shader(const VkShaderModule shader_module): shader_module(shader_module) {}

Shader::Shader(const VkShaderModule shader_module, const ShaderStage stage): shader_module(shader_module), stage(stage), entry_point("") {}

Shader::Shader(const VkShaderModule shader_module, const ShaderStage stage, const std::string& entry_point):
    shader_module(shader_module), stage(stage), entry_point(entry_point) {}

void Shader::set_stage(const ShaderStage stage) {
    this->stage = stage;
}

void Shader::set_entry_point(const std::string& entry_point) {
    this->entry_point = entry_point;
}

VkShaderStageFlagBits Shader::shader_stage_to_vulkan(ShaderStage stage) {
    switch (stage)
    {
    case ShaderStage::NONE:
        throw std::runtime_error("Invalid shader stage.");
    case ShaderStage::VERTEX:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case ShaderStage::FRAGMENT:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    default:
        throw std::runtime_error("Not implemented");
    }
}

ShaderStage Shader::get_stage() const {
    return stage;
}

const std::string& Shader::get_entry_point() const {
    return entry_point;
}

VkShaderModule Shader::get_shader_module() const {
    return shader_module;
}

}