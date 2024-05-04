#include "resources.hpp"
#include <vulkan/vulkan.h>

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

    Buffer create_buffer(const BufferInfo& info);
    Texture create_texture(const TextureInfo& info);

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
    uint32_t frame = 0;
    // flags
    uint32_t committed : 1;
    uint32_t need_submit : 1;

    StagingBuffer* acquire_staging_buffer(VkDeviceSize size);
    Buffer create_vk_buffer(const BufferInfo& info);
};

}
