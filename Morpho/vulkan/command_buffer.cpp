#include "command_buffer.hpp"
#include "vulkan/context.hpp"
#include <vulkan/vulkan_core.h>
#include "vulkan/resource_manager.hpp"
// Again, use it like this for now for simplicity
#include "common/draw_stream.hpp"

namespace Morpho::Vulkan {

VkCommandBuffer CommandBuffer::get_vulkan_handle() const {
    return command_buffer;
}

CommandBuffer::CommandBuffer(VkCommandBuffer command_buffer) {
    this->command_buffer = command_buffer;
}

void CommandBuffer::end_render_pass() {
    vkCmdEndRenderPass(command_buffer);
    current_render_pass = Handle<RenderPass>::null();
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
    ResourceManager* rm = ResourceManager::get();
    Texture src_texture = rm->get_texture(info.src_texture);
    Texture dst_texture = rm->get_texture(info.dst_texture);
    for (uint32_t i = 0; i < info.regions.size(); i++) {
        VkImageBlit& vk_region = regions[i];
        const TextureBlit& region = info.regions[i];
        vk_region.srcSubresource = {
            .aspectMask = src_texture.aspect,
            .mipLevel = region.src_subresource.mip_level,
            .baseArrayLayer = region.src_subresource.base_array_layer,
            .layerCount = region.src_subresource.layer_count,
        };
        vk_region.srcOffsets[0] = info.regions[i].src_offsets[0];
        vk_region.srcOffsets[1] = info.regions[i].src_offsets[1];
        vk_region.dstSubresource = {
            .aspectMask = dst_texture.aspect,
            .mipLevel = region.dst_subresource.mip_level,
            .baseArrayLayer = region.dst_subresource.base_array_layer,
            .layerCount = region.dst_subresource.layer_count,
        };
        vk_region.dstOffsets[0] = info.regions[i].dst_offsets[0];
        vk_region.dstOffsets[1] = info.regions[i].dst_offsets[1];
    }
    vkCmdBlitImage(
        command_buffer,
        src_texture.image,
        info.src_texture_layout,
        dst_texture.image,
        info.dst_texture_layout,
        info.regions.size(),
        regions,
        info.filter
    );
}

void CommandBuffer::bind_vertex_buffer(Handle<Buffer> vertex_buffer, uint32_t binding, VkDeviceSize offset) {
    VkBuffer vk_buffer = ResourceManager::get()->get_buffer(vertex_buffer).buffer;
    vkCmdBindVertexBuffers(command_buffer, binding, 1, &vk_buffer, &offset);
}

void CommandBuffer::bind_index_buffer(Handle<Buffer> index_buffer, VkIndexType index_type, VkDeviceSize offset) {
    VkBuffer vk_buffer = ResourceManager::get()->get_buffer(index_buffer).buffer;
    vkCmdBindIndexBuffer(command_buffer, vk_buffer, offset, index_type);
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
        Texture texture = ResourceManager::get()->get_texture(barrier.texture);
        VkImageMemoryBarrier& vk_barrier = image_barriers[i];
        vk_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        vk_barrier.image = texture.image;
        vk_barrier.oldLayout = barrier.old_layout;
        vk_barrier.newLayout = barrier.new_layout;
        vk_barrier.srcAccessMask = barrier.src_access;
        src_stages |= barrier.src_stages;
        vk_barrier.dstAccessMask = barrier.dst_access;
        dst_stages |= barrier.dst_stages;
        vk_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vk_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vk_barrier.subresourceRange.aspectMask = texture.aspect;
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
    Handle<RenderPass> render_pass,
    Framebuffer framebuffer,
    VkRect2D render_area,
    std::initializer_list<VkClearValue> clear_values
) {
    ResourceManager* rm = ResourceManager::get();
    VkRenderPassBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.clearValueCount = (uint32_t)clear_values.size();
    begin_info.pClearValues = clear_values.begin();
    begin_info.renderPass = rm->get_render_pass(render_pass).render_pass;
    begin_info.framebuffer = framebuffer.framebuffer;
    begin_info.renderArea = render_area;
    current_render_pass = render_pass;
    vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void CommandBuffer::bind_pipeline(Handle<Pipeline> pipeline) {
    vkCmdBindPipeline(this->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ResourceManager::get()->get_pipeline(pipeline).pipeline);
}

void CommandBuffer::bind_descriptor_set(Handle<DescriptorSet> set_handle) {
    DescriptorSet set = ResourceManager::get()->get_descriptor_set(set_handle);
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

void CommandBuffer::decode_stream(DrawPassInfo draw_pass_info) {
    ResourceManager* rm = ResourceManager::get();
    VkRenderPassBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.clearValueCount = (uint32_t)draw_pass_info.clear_values.size();
    begin_info.pClearValues = draw_pass_info.clear_values.data();
    begin_info.renderPass = rm->get_render_pass(draw_pass_info.render_pass).render_pass;
    begin_info.framebuffer = draw_pass_info.framebuffer.framebuffer;
    begin_info.renderArea = draw_pass_info.render_area;
    current_render_pass = draw_pass_info.render_pass;
    vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    const VkRect2D rect = draw_pass_info.render_area;
    set_viewport({
        .x = (float)rect.offset.x,
        .y = (float)rect.offset.y,
        .width = (float)rect.extent.width,
        .height =(float)rect.extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    });
    set_scissor(rect);
    bind_descriptor_set(draw_pass_info.global_ds);
    Span<const DrawStream::DrawCall> stream = make_const_span(
        (DrawStream::DrawCall*)draw_pass_info.stream.data(),
        draw_pass_info.stream.size() / sizeof(DrawStream::DrawCall)
    );
    DrawStream::DrawCall current_dc = DrawStream::DrawCall::null();
    VkCommandBuffer vk_cmd = this->command_buffer;
    for (uint32_t i = 0; i < stream.size(); i++) {
        const DrawStream::DrawCall& dc = stream[i];
        if (current_dc.pipeline != dc.pipeline) {
            vkCmdBindPipeline(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, rm->get_pipeline(dc.pipeline).pipeline);
            current_dc.pipeline = dc.pipeline;
        }
        for (uint32_t i = 0; i < 3; i++) {
            if (current_dc.descriptor_sets[i] != dc.descriptor_sets[i]) {
                DescriptorSet set = rm->get_descriptor_set(dc.descriptor_sets[i]);
                vkCmdBindDescriptorSets(
                    vk_cmd,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    set.pipeline_layout,
                    i + 1,
                    1,
                    &set.descriptor_set,
                    0,
                    nullptr
                );
                current_dc.descriptor_sets[i] = dc.descriptor_sets[i];
            }
        }
        if (current_dc.index_buffer != dc.index_buffer || current_dc.index_buffer_offset != dc.index_buffer_offset) {
            Buffer buffer = rm->get_buffer(dc.index_buffer);
            vkCmdBindIndexBuffer(command_buffer, buffer.buffer, dc.index_buffer_offset, VK_INDEX_TYPE_UINT16);
            current_dc.index_buffer = dc.index_buffer;
            current_dc.index_buffer_offset = dc.index_buffer_offset;
        }
        for (uint32_t i = 0; i < 4; i++) {
            if (
                current_dc.vertex_buffers[i] != dc.vertex_buffers[i]
                || current_dc.vertex_buffer_offsets[i] != dc.vertex_buffer_offsets[i]
            ) {
                Buffer buffer = rm->get_buffer(dc.vertex_buffers[i]);
                uint64_t offset = dc.vertex_buffer_offsets[i];
                vkCmdBindVertexBuffers(command_buffer, i, 1, &buffer.buffer, &offset);
                current_dc.vertex_buffers[i] = dc.vertex_buffers[i];
                current_dc.vertex_buffer_offsets[i] = dc.vertex_buffer_offsets[i];
            }
        }
        vkCmdDrawIndexed(vk_cmd, dc.index_count, 1, dc.index_offset, 0, 0);
    }
    vkCmdEndRenderPass(vk_cmd);
}

}
