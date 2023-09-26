#include "command_buffer.hpp"
#include "context.hpp"
#include <vulkan/vulkan_core.h>

namespace Morpho::Vulkan {

VkCommandBuffer CommandBuffer::get_vulkan_handle() const {
    return command_buffer;
}

CommandBuffer::CommandBuffer(VkCommandBuffer command_buffer, Context* context) {
    this->command_buffer = command_buffer;
    this->context = context;
}

void CommandBuffer::end_render_pass() {
    vkCmdEndRenderPass(command_buffer);
    current_render_pass = RenderPass();
}

void CommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
    vkCmdDraw(command_buffer, vertex_count, instance_count, first_vertex, first_instance);
}

void CommandBuffer::draw_indexed(
    uint32_t index_count,
    uint32_t instance_count,
    uint32_t first_index,
    int32_t vertex_offset,
    uint32_t first_instance
) {
    vkCmdDrawIndexed(command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void CommandBuffer::bind_vertex_buffer(Buffer vertex_buffer, uint32_t binding, VkDeviceSize offset) {
    auto vk_buffer = vertex_buffer.buffer;
    vkCmdBindVertexBuffers(command_buffer, binding, 1, &vk_buffer, &offset);
}

void CommandBuffer::bind_index_buffer(Buffer index_buffer, VkIndexType index_type, VkDeviceSize offset) {
    vkCmdBindIndexBuffer(command_buffer, index_buffer.buffer, offset, index_type);
}

void CommandBuffer::copy_buffer(Buffer source, Buffer destination, VkDeviceSize size) const {
    auto dst = destination.buffer;
    auto src = source.buffer;
    VkBufferCopy region;
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = size;
    vkCmdCopyBuffer(command_buffer, src, dst, 1, &region);
}

void CommandBuffer::copy_buffer_to_image(Buffer source, Texture destination, VkExtent3D extent) const {
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = extent;

    vkCmdCopyBufferToImage(
        command_buffer,
        source.buffer,
        destination.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );
}

void CommandBuffer::image_barrier(
    const Texture& texture,
    VkImageAspectFlags aspect,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    VkPipelineStageFlags src_stages,
    VkAccessFlags src_access,
    VkPipelineStageFlags dst_stages,
    VkAccessFlags dst_access,
    uint32_t layer_count
) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    barrier.image = texture.image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = layer_count;
    vkCmdPipelineBarrier(
        command_buffer,
        src_stages,
        dst_stages,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );
}

void CommandBuffer::buffer_barrier(
    const Buffer& buffer,
    VkPipelineStageFlags src_stages,
    VkAccessFlags src_access,
    VkPipelineStageFlags dst_stages,
    VkAccessFlags dst_access,
    VkDeviceSize offset,
    VkDeviceSize size
) {
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.buffer = buffer.buffer;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.offset = offset;
    barrier.size = size;
    vkCmdPipelineBarrier(
        command_buffer,
        src_stages,
        dst_stages,
        0,
        0,
        nullptr,
        1,
        &barrier,
        0,
        nullptr
    );
}

void CommandBuffer::set_viewport(VkViewport viewport) {
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
}

void CommandBuffer::set_scissor(VkRect2D scissor) {
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
}

void CommandBuffer::begin_render_pass(
    RenderPass render_pass,
    Framebuffer framebuffer,
    VkRect2D render_area,
    std::initializer_list<VkClearValue> clear_values
) {
    VkRenderPassBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.clearValueCount = (uint32_t)clear_values.size();
    begin_info.pClearValues = clear_values.begin();
    begin_info.renderPass = render_pass.render_pass;
    begin_info.framebuffer = framebuffer.framebuffer;
    begin_info.renderArea = render_area;
    current_render_pass = render_pass;
    vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void CommandBuffer::bind_pipeline(const Pipeline& pipeline) {
    this->pipeline = pipeline;
    vkCmdBindPipeline(this->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
}

void CommandBuffer::bind_descriptor_set(const DescriptorSet& set) {
    vkCmdBindDescriptorSets(
        this->command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        set.pipeline_layout,
        set.set_index,
        1,
        &set.descriptor_set,
        0,
        nullptr
    );
}

}
