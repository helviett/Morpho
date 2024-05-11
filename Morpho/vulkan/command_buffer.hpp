#pragma once
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include "limits.hpp"
#include "resources.hpp"

namespace Morpho::Vulkan {

class Context;

struct TextureBarrier {
    Handle<Texture> texture;
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

struct TextureBlit {
    TextureSubresource src_subresource;
    VkOffset3D src_offsets[2];
    TextureSubresource dst_subresource;
    VkOffset3D dst_offsets[2];
};

struct BlitInfo {
    Handle<Texture> src_texture;
    VkImageLayout src_texture_layout;
    Handle<Texture> dst_texture;
    VkImageLayout dst_texture_layout;
    VkFilter filter = VK_FILTER_LINEAR;
    Span<const TextureBlit> regions;
};

class CommandBuffer {
public:
    CommandBuffer(VkCommandBuffer command_buffer);
    VkCommandBuffer get_vulkan_handle() const;
    void end_render_pass();
    void bind_vertex_buffer(Handle<Buffer> vertex_buffer, uint32_t binding, VkDeviceSize offset = 0);
    void bind_index_buffer(Handle<Buffer> index_buffer, VkIndexType index_type, VkDeviceSize offset = 0);
    void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);
    void draw_indexed(
        uint32_t index_count,
        uint32_t instance_count,
        uint32_t first_index,
        int32_t vertex_offset,
        uint32_t first_instance
    );
    void blit(const BlitInfo& info);
    void copy_buffer(Buffer source, Buffer destination, VkDeviceSize size) const;
    void copy_buffer(Buffer source, Buffer destination, VkBufferCopy copy) const;
    void copy_buffer_to_image(Buffer source, Texture destination, VkExtent3D extent) const;
    void copy_buffer_to_image(Buffer source, Texture destination, BufferTextureCopyRegion region) const;
    void barrier(
        Span<const TextureBarrier> texture_barriers,
        Span<const BufferBarrier> buffer_barriers
    );
    void begin_render_pass(
        Handle<RenderPass> render_pass,
        Framebuffer framebuffer,
        VkRect2D render_area,
        std::initializer_list<VkClearValue> clear_values
    );
    void bind_pipeline(Handle<Pipeline> pipeline);

    void reset();
    void set_viewport(VkViewport viewport);
    void set_scissor(VkRect2D scissor);
    void bind_descriptor_set(Handle<DescriptorSet> set_handle);
private:
    VkCommandBuffer command_buffer;
    Handle<RenderPass> current_render_pass = Handle<RenderPass>::null();
    Context* context;
    DescriptorSet descriptor_sets[Limits::MAX_DESCRIPTOR_SET_COUNT];
};

}
