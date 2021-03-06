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
#include "render_pass.hpp"
#include "render_pass.hpp"
#include "framebuffer.hpp"
#include "shader.hpp"
#include "pipeline_state.hpp"
#include "pipeline.hpp"
#include "vma.hpp"
#include "buffer.hpp"
#include "limits.hpp"
#include "image.hpp"
#include "image_view.hpp"
#include "sampler.hpp"
#include "render_pass_layout.hpp"

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
    RenderPassLayout acquire_render_pass_layout(const RenderPassLayoutInfo& info);
    RenderPass acquire_render_pass(const RenderPassInfo& info);
    Framebuffer acquire_framebuffer(const FramebufferInfo& info);
    Pipeline acquire_pipeline(PipelineState &info, RenderPass& render_pass, uint32_t subpass);
    // Keep Vulkan and VMA flags for now for simplicity and prototyping speed.
    Buffer acquire_buffer(VkDeviceSize size, VkBufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage);
    Buffer acquire_staging_buffer(VkDeviceSize size, VkBufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage);
    void release_buffer(Buffer buffer);
    void map_memory(VmaAllocation allocation, void **map);
    void unmap_memory(VmaAllocation allocation);
    PipelineLayout acquire_pipeline_layout(ResourceSet sets[Limits::MAX_DESCRIPTOR_SET_COUNT]);
    DescriptorSet acquire_descriptor_set(DescriptorSetLayout descriptor_set_layout);
    void update_descriptor_set(DescriptorSet descriptor_set, ResourceSet resource_set);
    Image acquire_image(VkExtent3D extent, VkFormat format, VkImageUsageFlags image_usage, VmaMemoryUsage memory_usage);
    Image acquire_temporary_image(VkExtent3D extent, VkFormat format, VkImageUsageFlags image_usage, VmaMemoryUsage memory_usage);
    void release_image(Image image);
    ImageView create_image_view(VkFormat format, Image& image, VkImageAspectFlags aspect);
    ImageView create_temporary_image_view(VkFormat format, Image& image, VkImageAspectFlags aspect);
    void destroy_image_view(ImageView image_view);
    Sampler acquire_sampler(VkSamplerAddressMode address_mode, VkFilter filter);
    Sampler acquire_sampler(
        VkSamplerAddressMode address_mode_u,
        VkSamplerAddressMode address_mode_v,
        VkSamplerAddressMode address_mode_w,
        VkFilter min_filter,
        VkFilter mag_filter
    );


    // public WSI stuff
    ImageView get_swapchain_image_view() const;
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
    VkRenderPass create_render_pass(const RenderPassInfo& info);
    static uint32_t score_gpu(VkPhysicalDevice gpu);
    VkResult try_create_device();
    void retrieve_queues();
    FrameContext& get_current_frame_context();

    // WSI stuff that will soon migrate somewhere
    GLFWwindow* window;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    std::vector<Image> swapchain_images;
    std::vector<ImageView> swapchain_image_views;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    void create_surface();
    void create_swapchain();
    // end of WSI stuff
};

}
