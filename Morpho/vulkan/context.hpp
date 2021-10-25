#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <optional>

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

    void init();
private:
#ifdef NDEBUG
    const bool enable_validation_layers = false;
#else
    const bool enable_validation_layers = true;
#endif
    const std::vector<const char*> validation_layers = {
        "VK_LAYER_KHRONOS_validation"
    };
    VkDebugUtilsMessengerEXT debug_messenger;
    VkInstance instance;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphics_queue;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    );

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphics_family;

        bool is_complete() {
            return graphics_family.has_value();
        }
    };

    void create_instance();
    void create_device();
    std::vector<const char*> get_required_extensions();
    bool are_validation_layers_supported();
    void setup_debug_messenger();
    void select_physical_device();
    bool is_physical_device_suitable(const VkPhysicalDevice& device);
    QueueFamilyIndices retrieve_queue_family_indices(VkPhysicalDevice device);
};

}
