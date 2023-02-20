#pragma once
#include <vulkan/vulkan.h>
#include "render_pass.hpp"
#include "pipeline_state.hpp"
#include "buffer.hpp"
#include "shader.hpp"
#include "resource_set.hpp"
#include "pipeline.hpp"
#include "descriptor_set.hpp"
#include "limits.hpp"
#include "image.hpp"
#include "framebuffer.hpp"

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

    // Will turn into set_shader
    void add_shader(const Shader shader);
    void clear_shaders();
    void add_vertex_attribute_description(uint32_t binding, uint32_t location, VkFormat format, uint32_t offset);
    void add_vertex_binding_description(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate);
    void set_uniform_buffer(uint32_t set, uint32_t binding, Buffer buffer, VkDeviceSize offset, VkDeviceSize range);
    void set_combined_image_sampler(
        uint32_t set,
        uint32_t binding,
        ImageView image_view,
        Sampler sampler,
        VkImageLayout image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
    void set_depth_state(VkBool32 test_enable, VkBool32 write_enable, VkCompareOp compare_op);
    void clear_vertex_attribute_descriptions();
    void clear_vertex_binding_descriptions();
    void set_front_face(VkFrontFace front_face);
    void set_cull_mode(VkCullModeFlags cull_mode);
    void set_topology(VkPrimitiveTopology topology);
    void enable_depth_bias(float depth_bias_constant_factor, float depth_bias_slope_factor);
    void disable_depth_bias();
    void enable_blending(
        VkBlendFactor src_color_blend_factor,
        VkBlendFactor dst_color_blend_factor,
        VkBlendOp color_blend_op,
        VkBlendFactor src_alpha_blend_factor,
        VkBlendFactor dst_alpha_blend_factor,
        VkBlendOp alpha_blend_op
    );
    void disable_blending();
    void reset();
    void set_viewport(VkViewport viewport);
    void set_scissor(VkRect2D scissor);
private:
    VkCommandBuffer command_buffer;
    RenderPass current_render_pass = RenderPass();
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
