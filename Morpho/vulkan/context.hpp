#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <optional>
#include <set>
#include <fstream>
#include <filesystem>

namespace Morpho::Vulkan {

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
private:
#ifdef NDEBUG
    const bool enable_validation_layers = false;
#else
    const bool enable_validation_layers = true;
#endif
    const std::vector<const char*> validation_layers = {
        "VK_LAYER_KHRONOS_validation"
    };
    const std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    VkDebugUtilsMessengerEXT debug_messenger;
    VkInstance instance;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue present_queue;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchain_images;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    std::vector<VkImageView> swapchain_image_views;
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkRenderPass render_pass;
    std::vector<VkFramebuffer> framebuffers;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkSemaphore render_semaphore, present_semaphore;
    VkFence render_fence;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    );

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphics_family;
        std::optional<uint32_t> present_family;

        bool are_complete() {
            return graphics_family.has_value() && present_family.has_value();
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
    };

    void create_instance();
    void create_device();
    std::vector<const char*> get_required_extensions();
    bool are_validation_layers_supported();
    bool are_device_extensions_supported(VkPhysicalDevice device);
    void setup_debug_messenger();
    void select_physical_device();
    bool is_physical_device_suitable(const VkPhysicalDevice& device);
    void create_surface(GLFWwindow* window);
    QueueFamilyIndices retrieve_queue_family_indices(VkPhysicalDevice device);
    SwapChainSupportDetails query_swapchain_support_details(VkPhysicalDevice device);
    VkPresentModeKHR choose_present_mode(std::vector<VkPresentModeKHR>& modes);
    VkSurfaceFormatKHR choose_surface_format(std::vector<VkSurfaceFormatKHR>& formats);
    VkExtent2D choose_swapchain_extent(const VkSurfaceCapabilitiesKHR& capabilities);
    void create_swapchain();
    void create_image_views();
    VkShaderModule create_shader_module(const std::vector<char>& bytes);
    void create_graphics_pipeline();
    void create_render_pass();
    void create_framebuffers();
    void create_command_pool();
    void create_command_buffers();
    void create_sync_structures();

    static std::vector<char> read_all_bytes(const std::string& filename);
};

}
