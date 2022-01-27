#pragma once
#include <vulkan/vulkan.h>
#include "render_pass_info.hpp"
#include "render_pass.hpp"
#include "pipeline_info.hpp"
#include "buffer.hpp"

namespace Morpho::Vulkan {

class Context;

class CommandBuffer {
public:
    CommandBuffer(VkCommandBuffer command_buffer, Context* context);
    VkCommandBuffer get_vulkan_handle() const;
    void begin_render_pass(RenderPassInfo& render_pass_info);
    void end_render_pass();
    void bind_pipeline(PipelineInfo& info);
    void bind_vertex_buffer(Buffer vertex_buffer, uint32_t binding, VkDeviceSize offset = 0);
    void bind_index_buffer(Buffer index_buffer, VkIndexType index_type, VkDeviceSize offset = 0);
    void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) const;
    void draw_indexed(
        uint32_t index_count,
        uint32_t instance_count,
        uint32_t first_index,
        int32_t vertex_offset,
        uint32_t first_instance
    ) const;
private:
    VkCommandBuffer command_buffer;
    RenderPass current_render_pass = RenderPass(VK_NULL_HANDLE);
    Context* context;
};

}
