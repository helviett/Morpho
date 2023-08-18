#pragma once

#include <vulkan/vulkan.h>
#define VK_USE_PLATFORM_WIN32_KHR5
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <array>
#include <functional>
#include "resources.hpp"
#include "command_buffer.hpp"
#include "vma.hpp"
#include "limits.hpp"
#include "../common/resource_cache.hpp"
#include "descriptor_pool.hpp"

namespace Morpho::Vulkan {

#define VK_CHECK(result, message) \
    if (result != VK_SUCCESS) { \
        throw std::runtime_error(message);\
    }

#define MAX_FRAME_CONTEXTS 3

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger
);

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator
);

inline bool is_depth_format(VkFormat format) {
    return format == VK_FORMAT_D16_UNORM
        || format == VK_FORMAT_D16_UNORM_S8_UINT
        || format == VK_FORMAT_D24_UNORM_S8_UINT
        || format == VK_FORMAT_D32_SFLOAT
        || format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

class Context {
public:
    Context() = default;
    ~Context();
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
    Context(Context &&) = delete;
    Context &operator=(Context &&) = delete;

    void init(GLFWwindow *window);
    void set_frame_context_count(uint32_t count);
    void begin_frame();
    void end_frame();
    CommandBuffer acquire_command_buffer();
    void submit(CommandBuffer command_buffer);
    void flush(CommandBuffer command_buffer);
    Shader acquire_shader(char* data, uint32_t size, Morpho::Vulkan::ShaderStage stage);
    RenderPassLayout acquire_render_pass_layout(const RenderPassLayoutInfo& info);
    RenderPass acquire_render_pass(const RenderPassInfo& info);
    Framebuffer acquire_framebuffer(const FramebufferInfo& info);
    Pipeline create_pipeline(PipelineInfo &pipeline_info);
    // Keep Vulkan and VMA flags for now for simplicity and prototyping speed.
    Buffer acquire_buffer(VkDeviceSize size, VkBufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage);
    Buffer acquire_buffer(VkDeviceSize size, VkBufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage, void* data, uint64_t data_size);
    Buffer acquire_staging_buffer(VkDeviceSize size, VkBufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage);
    Buffer acquire_staging_buffer(VkDeviceSize size, VkBufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage, void* data, uint64_t data_size);
    void release_buffer(Buffer buffer);
    void update_buffer(const Buffer buffer, const void* data, uint64_t size);
    void map_memory(VmaAllocation allocation, void **map);
    void unmap_memory(VmaAllocation allocation);
    PipelineLayout acquire_pipeline_layout(ResourceSet sets[Limits::MAX_DESCRIPTOR_SET_COUNT]);
    PipelineLayout create_pipeline_layout(const PipelineLayoutInfo& pipeline_layout_info);
    void destroy_pipline_layout(const PipelineLayout& pipeline_layout);
    DescriptorSet acquire_descriptor_set(VkDescriptorSetLayout descriptor_set_layout);
    void update_descriptor_set(DescriptorSet descriptor_set, ResourceSet resource_set);
    Texture create_texture(
        VkExtent3D extent,
        VkFormat format,
        VkImageUsageFlags image_usage,
        VmaMemoryUsage memory_usage,
        uint32_t array_layers = 1,
        VkImageCreateFlags flags = 0
    );
    Texture create_temporary_texture(
        VkExtent3D extent,
        VkFormat format,
        VkImageUsageFlags image_usage,
        VmaMemoryUsage memory_usage,
        uint32_t array_layers = 1,
        VkImageCreateFlags flags = 0
    );
    Texture create_texture_view(
        const Texture& texture,
        uint32_t base_array_layer,
        uint32_t layer_count
    );
    Texture create_temporary_texture_view(
        const Texture& texture,
        uint32_t base_array_layer,
        uint32_t layer_count
    );
    void destroy_texture(Texture texture);
    Sampler acquire_sampler(
        VkSamplerAddressMode address_mode,
        VkFilter filter,
        VkBool32 compare_enable = VK_FALSE,
        VkCompareOp compare_op = VK_COMPARE_OP_NEVER
    );
    Sampler acquire_sampler(
        VkSamplerAddressMode address_mode_u,
        VkSamplerAddressMode address_mode_v,
        VkSamplerAddressMode address_mode_w,
        VkFilter min_filter,
        VkFilter mag_filter,
        VkBool32 compare_enable = VK_FALSE,
        VkCompareOp compare_op = VK_COMPARE_OP_NEVER
    );

    // public WSI stuff
    Texture get_swapchain_texture() const;
    VkExtent2D get_swapchain_extent() const;
    VkFormat get_swapchain_format() const;
    // end of WSI stuff

private:
#ifdef NDEBUG
    const bool enable_validation_layers = false;
#else
    const bool enable_validation_layers = true;
#endif

    uint32_t frame_context_count = 1;
    uint32_t frame_context_index = 0;
    uint32_t swapchain_image_index;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkPhysicalDevice gpu = VK_NULL_HANDLE;
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkDescriptorSetLayout empty_descriptor_set_layout;
    VkQueue graphics_queue;
    uint32_t graphics_queue_family_index;
    VmaAllocator allocator;
    ResourceCache<VkPipelineLayout> pipeline_layout_cache;
    ResourceCache<VkDescriptorSetLayout> descriptor_set_layout_cache;
    ResourceCache<VkRenderPass> render_pass_cache;

    struct FrameContext {
        // Stays here for a while for simplicity
        std::vector<std::function<void(void)>> destructors;
        std::vector<DescriptorPool> descriptor_pools;
        uint32_t current_descriptor_pool_index = 0;
        VkCommandPool command_pool;
        VkFence render_fence;
        VkSemaphore render_semaphore, present_semaphore;

        FrameContext() = default;
        FrameContext(const FrameContext &) = delete;
        FrameContext &operator=(const FrameContext &) = delete;
        FrameContext(FrameContext &&) = delete;
        FrameContext &operator=(FrameContext &&) = delete;
    } frame_contexts[MAX_FRAME_CONTEXTS];


    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    );
    VkPhysicalDevice select_gpu();
    VkResult try_create_instance(std::vector<const char*>& extensions, std::vector<const char*>& layers);
    VkDebugUtilsMessengerCreateInfoEXT get_default_messenger_create_info();
    VkRenderPass create_render_pass(const RenderPassInfo& info);
    static uint32_t score_gpu(VkPhysicalDevice gpu);
    VkResult try_create_device();
    void retrieve_queues();
    FrameContext& get_current_frame_context();
    // The safest way to release resources is on the start of
    // the frame context they were last used in.
    void release_buffer_on_frame_begin(Buffer buffer);
    void release_texture_on_frame_begin(Texture image);

    // WSI stuff that will soon migrate somewhere
    GLFWwindow* window;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    std::vector<Texture> swapchain_textures;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    void create_surface();
    void create_swapchain();
    // end of WSI stuff
};

}
