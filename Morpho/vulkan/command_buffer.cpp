#include "command_buffer.hpp"
#include "vulkan/context.hpp"
#include <vulkan/vulkan_core.h>

namespace Morpho::Vulkan {

VkCommandBuffer CommandBuffer::get_vulkan_handle() const {
    return command_buffer;
}

CommandBuffer::CommandBuffer(VkCommandBuffer command_buffer) {
    this->command_buffer = command_buffer;
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

void CommandBuffer::blit(const BlitInfo& info) {
    VkImageBlit regions[128]{};
    for (uint32_t i = 0; i < info.regions.size(); i++) {
        VkImageBlit& vk_region = regions[i];
        const TextureBlit& region = info.regions[i];
        vk_region.srcSubresource = {
            .aspectMask = info.src_texture.aspect,
            .mipLevel = region.src_subresource.mip_level,
            .baseArrayLayer = region.src_subresource.base_array_layer,
            .layerCount = region.src_subresource.layer_count,
        };
        vk_region.srcOffsets[0] = info.regions[i].src_offsets[0];
        vk_region.srcOffsets[1] = info.regions[i].src_offsets[1];
        vk_region.dstSubresource = {
            .aspectMask = info.dst_texture.aspect,
            .mipLevel = region.dst_subresource.mip_level,
            .baseArrayLayer = region.dst_subresource.base_array_layer,
            .layerCount = region.dst_subresource.layer_count,
        };
        vk_region.dstOffsets[0] = info.regions[i].dst_offsets[0];
        vk_region.dstOffsets[1] = info.regions[i].dst_offsets[1];
    }
    vkCmdBlitImage(
        command_buffer,
        info.src_texture.image,
        info.src_texture_layout,
        info.dst_texture.image,
        info.dst_texture_layout,
        info.regions.size(),
        regions,
        info.filter
    );
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

void CommandBuffer::copy_buffer(Buffer source, Buffer destination, VkBufferCopy copy) const {
    auto dst = destination.buffer;
    auto src = source.buffer;
    vkCmdCopyBuffer(command_buffer, src, dst, 1, &copy);
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

void CommandBuffer::copy_buffer_to_image(Buffer source, Texture destination, BufferTextureCopyRegion region) const {
    VkBufferImageCopy vk_region{};
    vk_region.bufferOffset = region.buffer_offset;
    vk_region.bufferRowLength = 0;
    vk_region.bufferImageHeight = 0;
    vk_region.imageSubresource.aspectMask = derive_aspect(destination.format);
    vk_region.imageSubresource.mipLevel = region.texture_subresource.mip_level;
    vk_region.imageSubresource.baseArrayLayer = region.texture_subresource.base_array_layer;
    vk_region.imageSubresource.layerCount = region.texture_subresource.layer_count;
    vk_region.imageOffset = region.texture_offset;
    vk_region.imageExtent = region.texture_extent;

    vkCmdCopyBufferToImage(
        command_buffer,
        source.buffer,
        destination.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &vk_region
    );
}

void CommandBuffer::barrier(
    Span<const TextureBarrier> texture_barriers,
    Span<const BufferBarrier> buffer_barriers
) {
    VkPipelineStageFlags src_stages{};
    VkPipelineStageFlags dst_stages{};
    VkMemoryBarrier memory_barrier{};
    memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    // TODO: alloca
    VkImageMemoryBarrier image_barriers[128]{};
    assert(texture_barriers.size() <= 128);
    // AFAIK no driver takes advantage over VkBufferBarrier
    // so just merge everything into VkMemoryBarrier and ignore offset and size.
    for (uint32_t i = 0; i < buffer_barriers.size(); i++) {
        src_stages |= buffer_barriers[i].src_stages;
        memory_barrier.srcAccessMask |= buffer_barriers[i].src_access;
        dst_stages |= buffer_barriers[i].dst_stages;
        memory_barrier.dstAccessMask |= buffer_barriers[i].dst_access;
    }
    for (uint32_t i = 0; i < texture_barriers.size(); i++) {
        const TextureBarrier& barrier = texture_barriers[i];
        VkImageMemoryBarrier& vk_barrier = image_barriers[i];
        vk_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        vk_barrier.image = barrier.texture.image;
        vk_barrier.oldLayout = barrier.old_layout;
        vk_barrier.newLayout = barrier.new_layout;
        vk_barrier.srcAccessMask = barrier.src_access;
        src_stages |= barrier.src_stages;
        vk_barrier.dstAccessMask = barrier.dst_access;
        dst_stages |= barrier.dst_stages;
        vk_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vk_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vk_barrier.subresourceRange.aspectMask = barrier.texture.aspect;
        vk_barrier.subresourceRange.baseMipLevel = barrier.base_mip_level;
        vk_barrier.subresourceRange.levelCount = barrier.mip_level_count;
        vk_barrier.subresourceRange.baseArrayLayer = barrier.base_layer;
        vk_barrier.subresourceRange.layerCount = barrier.layer_count;
    }
    uint32_t memory_barrier_count = memory_barrier.srcAccessMask != 0 || memory_barrier.dstAccessMask != 0 ? 1 : 0;
    vkCmdPipelineBarrier(
         command_buffer,
         src_stages,
         dst_stages,
         0,
         memory_barrier_count,
         memory_barrier_count != 0 ? &memory_barrier : nullptr,
         0,
         nullptr,
         texture_barriers.size(),
         image_barriers
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
