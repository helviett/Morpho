#pragma once
#include <vulkan/vulkan.h>
#include "render_pass_info.hpp"
#include "render_pass.hpp"
#include "pipeline_state.hpp"
#include "buffer.hpp"
#include "shader.hpp"
#include "resource_set.hpp"
#include "pipeline.hpp"
#include "descriptor_set.hpp"
#include "limits.hpp"

namespace Morpho::Vulkan {

class Context;

class CommandBuffer {
public:
    CommandBuffer(VkCommandBuffer command_buffer, Context* context);
    VkCommandBuffer get_vulkan_handle() const;
    void begin_render_pass(RenderPassInfo& render_pass_info);
    void end_render_pass();
    void bind_vertex_buffer(Buffer vertex_buffer, uint32_t binding, VkDeviceSize offset = 0);
    void bind_index_buffer(Buffer index_buffer, VkIndexType index_type, VkDeviceSize offset = 0);
    void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);
    void draw_indexed(
        uint32_t index_count,
        uint32_t instance_count,
        uint32_t first_index,
        int32_t vertex_offset,
        uint32_t first_instance
    );
    void copy_buffer(Buffer source, Buffer destination, VkDeviceSize size) const;

    // Will turn into set_shader
    void add_shader(const Shader shader);
    void add_vertex_attribute_description(uint32_t binding, uint32_t location, VkFormat format, uint32_t offset);
    void add_vertex_binding_description(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate);
    void set_uniform_buffer(uint32_t set, uint32_t binding, Buffer buffer, VkDeviceSize offset, VkDeviceSize range);
private:
    VkCommandBuffer command_buffer;
    RenderPass current_render_pass = RenderPass(VK_NULL_HANDLE);
    PipelineState pipeline_state;
    Pipeline pipeline;
    Context* context;
    ResourceSet sets[Limits::MAX_DESCRIPTOR_SET_COUNT];
    DescriptorSet descriptor_sets[Limits::MAX_DESCRIPTOR_SET_COUNT];

    void flush_pipeline();
    void flush_descriptor_sets();
    void flush();
};

}
