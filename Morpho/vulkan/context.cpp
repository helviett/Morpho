#include "context.hpp"

namespace Morpho::Vulkan {

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator
) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger
) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void Context::init() {
    create_instance();
    select_physical_device();
    create_device();
}

void Context::create_device() {
    QueueFamilyIndices indices = retrieve_queue_family_indices(physical_device);

    VkPhysicalDeviceFeatures device_features{};

    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = indices.graphics_family.value();
    queue_create_info.queueCount = 1;
    float priority = 1.0f;
    queue_create_info.pQueuePriorities = &priority;

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    device_create_info.pEnabledFeatures = &device_features;

    if (vkCreateDevice(physical_device, &device_create_info, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Unable to create logical device.");
    }

    vkGetDeviceQueue(device, indices.graphics_family.value(), 0, &graphics_queue);
}

void Context::create_instance() {
    if (enable_validation_layers && !are_validation_layers_supported()) {
        throw std::runtime_error("Requested validation layers are not supported.");
    }

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Undefined";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Undefined";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    auto extensions = get_required_extensions();
    create_info.enabledExtensionCount = (uint32_t)extensions.size();
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledLayerCount = 0;

    VkDebugUtilsMessengerCreateInfoEXT messenger_create_info{};
    if (enable_validation_layers) {
        create_info.ppEnabledLayerNames = validation_layers.data();
        create_info.enabledLayerCount = (uint32_t)validation_layers.size();
        messenger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        messenger_create_info.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        messenger_create_info.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        messenger_create_info.pfnUserCallback = debug_callback;
        messenger_create_info.pUserData = nullptr;
        create_info.pNext = &messenger_create_info;
    }

    if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create instance.");
    }
}

std::vector<const char*> Context::get_required_extensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensionCount);

    for (size_t i = 0; i < glfwExtensionCount; i++) {
        extensions.push_back(glfwExtensions[i]);
    }

    if (enable_validation_layers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

bool Context::are_validation_layers_supported() {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    for (auto layer : validation_layers) {
        bool layer_found = false;
        for (const auto& available_layer : available_layers) {
            if (strcmp(layer, available_layer.layerName) == 0) {
                layer_found = true;
                break;
            }
        }
        if (!layer_found) {
            return false;
        }
    }

    return true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL Context::debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
) {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

void Context::setup_debug_messenger() {
    if (!enable_validation_layers) {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT messenger_create_info{};
    messenger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    messenger_create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    messenger_create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    messenger_create_info.pfnUserCallback = debug_callback;
    messenger_create_info.pUserData = nullptr;

    if (CreateDebugUtilsMessengerEXT(instance, &messenger_create_info, nullptr, &debug_messenger) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

Context::~Context() {
    if (enable_validation_layers) {
        DestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
    }
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
}

void Context::select_physical_device() {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
    for (const auto& device : devices) {
        if (is_physical_device_suitable(device)) {
            physical_device = device;
            break;
        }
    }
    if (physical_device == VK_NULL_HANDLE) {
        throw std::runtime_error("There is no suitable GPU.");
    }
}

bool Context::is_physical_device_suitable(const VkPhysicalDevice& device) {
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(device, &device_properties);
    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceFeatures(device, &device_features);
    auto queue_indicies = retrieve_queue_family_indices(device);

    return
        device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
        && queue_indicies.is_complete();
}

Context::QueueFamilyIndices Context::retrieve_queue_family_indices(VkPhysicalDevice device) {
    QueueFamilyIndices indices{};
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());
    for (int i = 0; i < queue_families.size(); i++) {
        const auto& queue_family = queue_families[i];
        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
        }
        if (indices.is_complete()) {
            break;
        }
    }

    return indices;
}

}
