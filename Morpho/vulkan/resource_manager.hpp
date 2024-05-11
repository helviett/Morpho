#pragma once
#include "resources.hpp"
#include <vulkan/vulkan.h>
#include "common/generational_arena.hpp"

namespace Morpho::Vulkan {

class Context;
struct CmdPool;
class CommandBuffer;

class ResourceManager {
public:
    friend class Context;
    ResourceManager(const ResourceManager &) = delete;
    ResourceManager &operator=(const ResourceManager &) = delete;
    ResourceManager(ResourceManager &&) = delete;
    ResourceManager &operator=(ResourceManager &&) = delete;
    ResourceManager() = delete;
    ~ResourceManager() = delete;

    static ResourceManager* create(Context* context);
    static void destroy(ResourceManager* rm);
    // Temp solution.
    static ResourceManager* get();

    Handle<Buffer> create_buffer(const BufferInfo& info, uint8_t**mapped_ptr = nullptr);
    Handle<Texture> create_texture(const TextureInfo& info);
    Handle<Texture> create_texture_view(
        Handle<Texture> texture,
        uint32_t base_array_layer,
        uint32_t layer_count
    );
    Handle<Shader> create_shader(char* data, uint32_t size, Morpho::Vulkan::ShaderStage stage);
    Handle<RenderPassLayout> create_render_pass_layout(const RenderPassLayoutInfo& info);
    Handle<RenderPass> create_render_pass(const RenderPassInfo& info);
    Handle<PipelineLayout> create_pipeline_layout(const PipelineLayoutInfo& pipeline_layout_info);
    Handle<DescriptorSet> create_descriptor_set(Handle<PipelineLayout> pipeline_layout, uint32_t set_index);
    Handle<Sampler> create_sampler(const SamplerInfo& info);
    Handle<Pipeline> create_pipeline(const PipelineInfo &pipeline_info);

    Handle<Texture> register_texture(Texture texture);

    void update_descriptor_set(
        Handle<DescriptorSet> descriptor_set,
        Span<const DescriptorSetUpdateRequest> update_requests
    );

    Buffer get_buffer(Handle<Buffer> handle);
    Texture get_texture(Handle<Texture> handle);
    Shader get_shader(Handle<Shader> handle);
    RenderPassLayout get_render_pass_layout(Handle<RenderPassLayout> handle);
    RenderPass get_render_pass(Handle<RenderPass> handle);
    PipelineLayout get_pipeline_layout(Handle<PipelineLayout> handle);
    DescriptorSet get_descriptor_set(Handle<DescriptorSet> handle);
    Sampler get_sampler(Handle<Sampler> handle);
    Pipeline get_pipeline(Handle<Pipeline> handle);

    void commit();
    void next_frame();
private:
    struct StagingBuffer {
        Buffer buffer;
        VkDeviceSize size;
        uint8_t* write_ptr;
        VkDeviceSize write_offset;
        VkDeviceSize used_offset;
        uint32_t frame_acquired;
    };

    static const uint64_t default_staging_buffer_size = 128 * 1024 * 1024;

    GenerationalArena<Buffer> buffers;
    GenerationalArena<Texture> textures;
    GenerationalArena<Shader> shaders;
    GenerationalArena<RenderPassLayout> render_pass_layouts;
    GenerationalArena<RenderPass> render_passes;
    GenerationalArena<PipelineLayout> pipeline_layouts;
    GenerationalArena<DescriptorSet> descriptor_sets;
    GenerationalArena<Sampler> samplers;
    GenerationalArena<Pipeline> pipelines;

    VmaAllocator allocator = VK_NULL_HANDLE;
    StagingBuffer* used_staging_buffers = nullptr;
    StagingBuffer* free_staging_buffers = nullptr;
    Context* context = nullptr;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    CmdPool* cmd_pool = VK_NULL_HANDLE;
    // Will become a handle soon.
    CommandBuffer* pre_cmd = nullptr;
    CommandBuffer* post_cmd = nullptr;
    VkImageMemoryBarrier* pre_barriers = nullptr;
    VkImageMemoryBarrier* post_barriers = nullptr;
    VkMemoryBarrier memory_barrier{};
    VkPipelineStageFlags src_stages{};
    VkPipelineStageFlags dst_stages{};
    VkDescriptorSetLayout empty_descriptor_set_layout;
    VkDescriptorPool empty_descriptor_pool;
    VkDescriptorSet empty_descriptor_set;
    uint32_t frame = 0;
    // flags
    uint32_t committed : 1;
    uint32_t need_submit : 1;

    StagingBuffer* acquire_staging_buffer(VkDeviceSize size);
    Buffer create_vk_buffer(const BufferInfo& info);
    VkRenderPass create_vk_render_pass(const RenderPassInfo& info, const RenderPassLayoutInfo& layout_info);
    void map_buffer_helper(Buffer* buffer);
    void unmap_buffer_helper(Buffer* buffer);
};

}
