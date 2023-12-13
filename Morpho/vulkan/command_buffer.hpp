#pragma once
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include "limits.hpp"
#include "resources.hpp"

namespace Morpho::Vulkan {

class Context;

struct TextureBarrier {
    Texture texture;
    VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout new_layout;
    uint32_t base_layer = 0;
    uint32_t layer_count = VK_REMAINING_ARRAY_LAYERS;
    uint32_t base_mip_level = 0;
    uint32_t mip_level_count = VK_REMAINING_MIP_LEVELS;
    VkPipelineStageFlags src_stages;
    VkAccessFlags src_access;
    VkPipelineStageFlags dst_stages;
    VkAccessFlags dst_access;
};

struct BufferBarrier {
    Buffer buffer;
    VkDeviceSize offset = 0;
    VkDeviceSize size = VK_WHOLE_SIZE;
    VkPipelineStageFlags src_stages;
    VkAccessFlags src_access;
    VkPipelineStageFlags dst_stages;
    VkAccessFlags dst_access;
};

class CommandBuffer {
public:
    CommandBuffer(VkCommandBuffer command_buffer, Context* context);
    VkCommandBuffer get_vulkan_handle() const;
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
    void copy_buffer_to_image(Buffer source, Texture destination, VkExtent3D extent) const;
    void barrier(
        Span<const TextureBarrier> texture_barriers,
        Span<const BufferBarrier> buffer_barriers
    );
    void begin_render_pass(
        RenderPass render_pass,
        Framebuffer framebuffer,
        VkRect2D render_area,
        std::initializer_list<VkClearValue> clear_values
    );
    void bind_pipeline(const Pipeline& pipeline);

    void reset();
    void set_viewport(VkViewport viewport);
    void set_scissor(VkRect2D scissor);
    void bind_descriptor_set(const DescriptorSet& set);
private:
    VkCommandBuffer command_buffer;
    RenderPass current_render_pass = RenderPass();
    Pipeline pipeline;
    Context* context;
    DescriptorSet descriptor_sets[Limits::MAX_DESCRIPTOR_SET_COUNT];
};

}
