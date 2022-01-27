#pragma once

#include <vulkan/vulkan.h>
#define VK_USE_PLATFORM_WIN32_KHR5
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <array>
#include <functional>
#include "command_buffer.hpp"
#include "render_pass_info.hpp"
#include "render_pass.hpp"
#include "framebuffer.hpp"
#include "shader.hpp"
#include "pipeline_info.hpp"
#include "pipeline.hpp"
#include "vma.hpp"
#include "buffer.hpp"

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

class Context {
public:
    Context() = default;
    Context(const Context &) = delete;
    ~Context();
	void operator=(const Context &) = delete;

    void init(GLFWwindow *window);
    void set_frame_context_count(uint32_t count);
    void begin_frame();
    void end_frame();
    CommandBuffer acquire_command_buffer();
    void submit(CommandBuffer command_buffer);
    void flush(CommandBuffer command_buffer);
    Shader acquire_shader(char* data, uint32_t size);
    RenderPass acquire_render_pass(RenderPassInfo& render_pass_info);
    Framebuffer acquire_framebuffer(RenderPass render_pass, RenderPassInfo& render_pass_info);
    Pipeline acquire_pipeline(PipelineInfo &info, RenderPass& render_pass, uint32_t subpass);
    // Keep Vulkan and VMA flags for now for simplicity and prototyping speed.
    Buffer acquire_buffer(VkDeviceSize size, VkBufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage);
    Buffer acquire_staging_buffer(VkDeviceSize size, VkBufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage);
    void release_buffer(Buffer buffer);
    void map_memory(VmaAllocation allocation, void **map);
    void unmap_memory(VmaAllocation allocation);

    // public WSI stuff
    VkImageView get_swapchain_image_view() const;
    VkExtent2D get_swapchain_extent() const;
    // end of WSI stuff

private:
#ifdef NDEBUG
    const bool enable_validation_layers = false;
#else
    const bool enable_validation_layers = true;
#endif

    uint32_t frame_context_count = 1;
    uint32_t frame_context_index;
    uint32_t swapchain_image_index;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkPhysicalDevice gpu = VK_NULL_HANDLE;
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphics_queue;
    uint32_t graphics_queue_family_index;
    VmaAllocator allocator;

    struct FrameContext {
        // Stays here for a while for simplicity
        std::vector<std::function<void(void)>> destructors;
        VkCommandPool command_pool;
        VkFence render_fence;
        VkSemaphore render_semaphore, present_semaphore;
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
    static uint32_t score_gpu(VkPhysicalDevice gpu);
    VkResult try_create_device();
    void retrieve_queues();
    FrameContext& get_current_frame_context();

    // WSI stuff that will soon migrate somewhere
    GLFWwindow* window;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    void create_surface();
    void create_swapchain();
    // end of WSI stuff
};

}
