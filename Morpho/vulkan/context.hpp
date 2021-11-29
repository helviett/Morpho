#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <optional>
#include <set>
#include <fstream>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>

namespace Morpho::Vulkan {

#define VK_CHECK(result, message) \
    if (result != VK_SUCCESS) { \
        throw std::runtime_error(message);\
    }

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

struct Vertex {
    glm::vec3 position;
    glm::vec2 uv;

    static VkVertexInputBindingDescription get_binding_description() {
        VkVertexInputBindingDescription desc{};
        desc.binding = 0;
        desc.stride = sizeof(Vertex);
        desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return desc;
    }

    static std::vector<VkVertexInputAttributeDescription> get_attributes_descriptions() {
        VkVertexInputAttributeDescription position_binding{};
        position_binding.binding = 0;
        position_binding.location = 0;
        position_binding.format = VK_FORMAT_R32G32B32_SFLOAT;
        position_binding.offset = 0;

        VkVertexInputAttributeDescription uv_binding{};
        uv_binding.binding = 0;
        uv_binding.location = 1;
        uv_binding.format = VK_FORMAT_R32G32B32_SFLOAT;
        uv_binding.offset = offsetof(Vertex, uv);

        std::vector<VkVertexInputAttributeDescription> descriptions(2);
        descriptions[0] = position_binding;
        descriptions[1] = uv_binding;

        return descriptions;
    }


};

class Context {
public:
    Context() = default;
    Context(const Context &) = delete;
    ~Context();
	void operator=(const Context &) = delete;


    void draw();
    void init(GLFWwindow *window);
private:
#ifdef NDEBUG
    const bool enable_validation_layers = false;
#else
    const bool enable_validation_layers = true;
#endif
    static const size_t FRAME_OVERLAP = 2;
    const std::vector<const char*> validation_layers = {
        "VK_LAYER_KHRONOS_validation"
    };
    const std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    struct CameraData {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 projection;
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
    VkBuffer vertex_buffer, index_buffer;
    VkDeviceMemory vertex_buffer_memory, index_buffer_memory;
    uint32_t frame_number;
    CameraData camera_data;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    VkImage texture_image;
    VkDeviceMemory texture_image_memory;
    VkImageView texture_image_view;
    VkSampler sampler;
    VkImage depth_image;
    VkDeviceMemory depth_image_memory;
    VkImageView depth_image_view;

    struct FrameData {
        VkSemaphore render_semaphore, present_semaphore;
        VkFence render_fence;
        VkCommandPool command_pool;
        VkCommandBuffer command_buffer;
        VkBuffer camera_uniform_buffer;
        VkDeviceMemory camera_buffer_memory;
        VkDescriptorSet descriptor_set;
    } frames[FRAME_OVERLAP];


    const std::vector<Vertex> vertices = {
        { { -0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f } },
        { { 0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f } },
        { { 0.5f, 0.5f, 0.0f }, { 0.0f, 1.0f } },
        { { -0.5f, 0.5f, 0.0f }, { 1.0f, 1.0f } },

        { { -0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f } },
        { { 0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f } },
        { { 0.5f, 0.5f, -0.5f }, { 0.0f, 1.0f } },
        { { -0.5f, 0.5f, -0.5f }, { 1.0f, 1.0f } }
    };

    const std::vector<uint16_t> indices = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4
    };


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
    void create_vertex_and_index_buffers();
    void create_buffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer &buffer,
        VkDeviceMemory &memory
    );
    void copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
    uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags flags);
    FrameData& get_current_frame_data();
    void create_descriptor_sets();
    void load_texture();
    void create_image(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage &image,
        VkDeviceMemory &memory
    );
    VkImageView create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect);
    void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void transition_image_layout(
        VkImage image,
        VkFormat format,
        VkImageLayout from,
        VkImageLayout to,
        VkAccessFlags src_access_mask,
        VkAccessFlags dst_access_mask,
        VkPipelineStageFlags src_stage,
        VkPipelineStageFlags dst_stage
    );
    void create_sampler();
    void create_depth_resources();
    VkFormat Context::find_depth_format();
    VkFormat Context::find_supported_format(
        const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    );

    static std::vector<char> read_all_bytes(const std::string& filename);
};

}
