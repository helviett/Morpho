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

void CommandBuffer::begin_render_pass(RenderPassInfo& render_pass_info) {
    current_render_pass = context->acquire_render_pass(render_pass_info);
    auto framebuffer = context->acquire_framebuffer(current_render_pass, render_pass_info);


    VkRenderPassBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.framebuffer = framebuffer.get_vulkan_handle();
    begin_info.clearValueCount = 1;
    begin_info.pClearValues = &render_pass_info.clear_value;
    begin_info.renderPass = current_render_pass.get_vulkan_handle();
    begin_info.renderArea.offset = { 0, 0 };
    begin_info.renderArea.extent = context->get_swapchain_extent();

    vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void CommandBuffer::end_render_pass() {
    vkCmdEndRenderPass(command_buffer);
    current_render_pass = RenderPass(VK_NULL_HANDLE);
}

void CommandBuffer::bind_pipeline(PipelineInfo& info) {
    auto pipeline = context->acquire_pipeline(info, current_render_pass, 0);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get_vulkan_handle());
}

void CommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) const {
    vkCmdDraw(command_buffer, vertex_count, instance_count, first_vertex, first_instance);
}

void CommandBuffer::bind_vertex_buffer(Buffer vertex_buffer, uint32_t binding) {
    auto buffer = vertex_buffer.get_buffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(command_buffer, binding, 1, &buffer, &offset);
}

}
