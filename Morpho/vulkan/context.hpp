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
#include "common/span.hpp"

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

inline VkImageAspectFlags derive_aspect(VkFormat format) {
    switch (format) {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D32_SFLOAT:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case VK_FORMAT_S8_UINT:
            return VK_IMAGE_ASPECT_STENCIL_BIT;
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

struct CmdPool {
public:
    friend class Context;
    CommandBuffer* allocate();
    void next_frame();
private:
    uint32_t current_frame;
    VkDevice device;
    VkCommandPool cmd_pools[MAX_FRAME_CONTEXTS];
};

class Context {
public:
    Context() = default;
    ~Context();
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
    Context(Context &&) = delete;
    Context &operator=(Context &&) = delete;

    friend class ResourceManager;
    friend struct CmdPool;

    void init(GLFWwindow *window);
    void set_frame_context_count(uint32_t count);
    void begin_frame();
    void end_frame();
    CommandBuffer* acquire_command_buffer();
    void submit(CommandBuffer* command_buffer);
    Framebuffer acquire_framebuffer(const FramebufferInfo& info);

    void create_cmd_pool(CmdPool** pool);
    void destroy_cmd_pool(CmdPool* pool);

    uint64_t get_uniform_buffer_alignment() const;

    // public WSI stuff
    Handle<Texture> get_swapchain_texture() const;
    VkExtent2D get_swapchain_extent() const;
    VkFormat get_swapchain_format() const;
    // end of WSI stuff

    // debug
    void wait_queue_idle();

    // temp for imgui
    void get_vulkans_guts(
        VkInstance* instance,
        VkPhysicalDevice* gpu,
        VkDevice* device,
        VkQueue* queue,
        uint32_t* queue_index,
        VkDescriptorPool* pool
    );

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
    VkQueue graphics_queue;
    uint32_t graphics_queue_family_index;
    VmaAllocator allocator;
    uint64_t min_uniform_buffer_offset_alignment;

    // Should make descriptor management explicit.
    VkDescriptorPool imgui_descriptor_pool;

    struct FrameContext {
        // Stays here for a while for simplicity
        std::vector<std::function<void(void)>> destructors;
        VkCommandPool command_pool;
        VkFence render_finished_fence;
        VkSemaphore render_semaphore, image_ready_semaphore;

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
    void query_gpu_properties();
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
    std::vector<Handle<Texture>> swapchain_texture_handles;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    void create_surface();
    void create_swapchain();
    // end of WSI stuff
};

}
