#pragma once
#include <vulkan/vulkan.h>
#include "limits.hpp"
#include "resources.hpp"
#include "resource_set.hpp"

namespace Morpho::Vulkan {

class Context;

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
    void copy_buffer_to_image(Buffer source, Image destination, VkExtent3D extent) const;
    void image_barrier(
        const Image& image,
        VkImageAspectFlags aspect,
        VkImageLayout old_layout,
        VkImageLayout new_layout,
        VkPipelineStageFlags src_stages,
        VkAccessFlags src_access,
        VkPipelineStageFlags dst_stages,
        VkAccessFlags dst_access,
        uint32_t layer_count = 1
    );
    void buffer_barrier(
        const Buffer& buffer,
        VkPipelineStageFlags src_stages,
        VkAccessFlags src_access,
        VkPipelineStageFlags dst_stages,
        VkAccessFlags dst_access,
        VkDeviceSize offset,
        VkDeviceSize size
    );
    void begin_render_pass(
        RenderPass render_pass,
        Framebuffer framebuffer,
        VkRect2D render_area,
        std::initializer_list<VkClearValue> clear_values
    );
    void bind_pipeline(Pipeline& pipeline);

    void set_uniform_buffer(uint32_t set, uint32_t binding, Buffer buffer, VkDeviceSize offset, VkDeviceSize range);
    void set_combined_image_sampler(
        uint32_t set,
        uint32_t binding,
        ImageView image_view,
        Sampler sampler,
        VkImageLayout image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
    void reset();
    void set_viewport(VkViewport viewport);
    void set_scissor(VkRect2D scissor);
private:
    VkCommandBuffer command_buffer;
    RenderPass current_render_pass = RenderPass();
    Pipeline pipeline;
    Context* context;
    ResourceSet sets[Limits::MAX_DESCRIPTOR_SET_COUNT];
    DescriptorSet descriptor_sets[Limits::MAX_DESCRIPTOR_SET_COUNT];

    void flush_descriptor_sets();
    void flush();
};

}
