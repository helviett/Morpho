#include "command_buffer.hpp"
#include "context.hpp"

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
    flush();
    vkCmdDraw(command_buffer, vertex_count, instance_count, first_vertex, first_instance);
}

void CommandBuffer::draw_indexed(
    uint32_t index_count,
    uint32_t instance_count,
    uint32_t first_index,
    int32_t vertex_offset,
    uint32_t first_instance
) {
    flush();
    vkCmdDrawIndexed(command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void CommandBuffer::bind_vertex_buffer(Buffer vertex_buffer, uint32_t binding, VkDeviceSize offset) {
    auto buffer = vertex_buffer.get_buffer();
    vkCmdBindVertexBuffers(command_buffer, binding, 1, &buffer, &offset);
}

void CommandBuffer::bind_index_buffer(Buffer index_buffer, VkIndexType index_type, VkDeviceSize offset) {
    auto buffer = index_buffer.get_buffer();
    vkCmdBindIndexBuffer(command_buffer, buffer, offset, index_type);
}

void CommandBuffer::copy_buffer(Buffer source, Buffer destination, VkDeviceSize size) const {
    auto dst = destination.get_buffer();
    auto src = source.get_buffer();
    VkBufferCopy region;
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = size;
    vkCmdCopyBuffer(command_buffer, src, dst, 1, &region);
}

void CommandBuffer::add_shader(const Shader shader) {
    pipeline_state.add_shader(shader);
}


void CommandBuffer::clear_shaders() {
    pipeline_state.clear_shaders();
}

void CommandBuffer::set_uniform_buffer(uint32_t set, uint32_t binding, Buffer buffer, VkDeviceSize offset, VkDeviceSize range) {
    sets[set].set_uniform_buffer(binding, buffer, offset, range);
}

void CommandBuffer::set_combined_image_sampler(
    uint32_t set,
    uint32_t binding,
    ImageView image_view,
    Sampler sampler,
    VkImageLayout image_layout
) {
    sets[set].set_combined_image_sampler(binding, image_view, sampler, image_layout);
}

void CommandBuffer::flush_pipeline() {
    bool is_pipeline_layout_dirty = false;
    for (uint32_t i = 0; i < Limits::MAX_DESCRIPTOR_SET_COUNT; i++) {
        is_pipeline_layout_dirty |= sets[i].get_is_layout_dirty();
    }
    if (is_pipeline_layout_dirty) {
        PipelineLayout pipeline_layout = context->acquire_pipeline_layout(sets);
        pipeline_state.set_pipeline_layout(pipeline_layout);
    }
    if (pipeline_state.get_and_clear_is_dirty()) {
        pipeline = context->acquire_pipeline(pipeline_state, current_render_pass, 0);
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get_pipeline());
    }
}

void CommandBuffer::flush_descriptor_sets() {
    bool is_pipeline_layout_dirty = false;
    for (uint32_t i = 0; i < Limits::MAX_DESCRIPTOR_SET_COUNT; i++) {
        is_pipeline_layout_dirty |= sets[i].get_is_layout_dirty();
    }
    for (uint32_t i = 0; i < Limits::MAX_DESCRIPTOR_SET_COUNT; i++) {
        if (
            is_pipeline_layout_dirty || descriptor_sets[i].get_descriptor_set() == VK_NULL_HANDLE
            || sets[i].get_is_contents_dirty()
        ) {
            descriptor_sets[i] = context->acquire_descriptor_set(
                pipeline_state.get_pipeline_layout().get_descriptor_set_layout(i)
            );
            sets[i].mark_all_dirty();
        }
        if (sets[i].get_is_contents_dirty()) {
            context->update_descriptor_set(descriptor_sets[i], sets[i]);
        }
        sets[i].clear_dirty_flags();
        auto descriptor_set = descriptor_sets[i].get_descriptor_set();
        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline_state.get_pipeline_layout().get_pipeline_layout(),
            i,
            1,
            &descriptor_set,
            0,
            nullptr
        );
    }
}

void CommandBuffer::flush() {
    flush_pipeline();
    flush_descriptor_sets();
}

void CommandBuffer::copy_buffer_to_image(Buffer source, Image destination, VkExtent3D extent) const {
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
        source.get_buffer(),
        destination.get_image(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );
}

void CommandBuffer::image_barrier(
    const Image& image,
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
    barrier.image = image.get_image();
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

void CommandBuffer::set_depth_state(VkBool32 test_enable, VkBool32 write_enable, VkCompareOp compare_op) {
    pipeline_state.set_depth_state(test_enable, write_enable, compare_op);
}

void CommandBuffer::set_front_face(VkFrontFace front_face) {
    pipeline_state.set_front_face(front_face);
}

void CommandBuffer::set_cull_mode(VkCullModeFlags cull_mode) {
    pipeline_state.set_cull_mode(cull_mode);
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
    barrier.buffer = buffer.get_buffer();
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


void CommandBuffer::set_topology(VkPrimitiveTopology topology) {
    pipeline_state.set_topology(topology);
}

void CommandBuffer::reset() {
    for (size_t i = 0; i < Limits::MAX_DESCRIPTOR_SET_COUNT; i++) {
        sets[i] = ResourceSet();
    }
    pipeline_state = PipelineState();
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
    begin_info.renderPass = render_pass.get_vulkan_handle();
    begin_info.framebuffer = framebuffer.get_vulkan_handle();
    begin_info.renderArea = render_area;
    current_render_pass = render_pass;
    vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void CommandBuffer::enable_depth_bias(float depth_bias_constant_factor, float depth_bias_slope_factor) {
    pipeline_state.enable_depth_bias(depth_bias_constant_factor, depth_bias_slope_factor);
}

void CommandBuffer::disable_depth_bias() {
    pipeline_state.disable_depth_bias();
}

void CommandBuffer::enable_blending(
    VkBlendFactor src_color_blend_factor,
    VkBlendFactor dst_color_blend_factor,
    VkBlendOp color_blend_op,
    VkBlendFactor src_alpha_blend_factor,
    VkBlendFactor dst_alpha_blend_factor,
    VkBlendOp alpha_blend_op
) {
    pipeline_state.enable_blending(
        src_color_blend_factor,
        dst_color_blend_factor,
        color_blend_op,
        src_alpha_blend_factor,
        dst_alpha_blend_factor,
        alpha_blend_op
    );
}

void CommandBuffer::disable_blending() {
    pipeline_state.disable_blending();
}

void CommandBuffer::set_vertex_format(VertexFormat vertex_format)
{
    pipeline_state.set_vertex_format(vertex_format);
}

}
