#include "context.hpp"

namespace Morpho::Vulkan {

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator
) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
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
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void Context::init(GLFWwindow* window) {
    create_instance();
    create_surface(window);
    select_physical_device();
    create_device();
    create_swapchain();
    create_image_views();
}

void Context::create_surface(GLFWwindow* window) {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Unable to create window surface.");
    }
}

void Context::create_device() {
    QueueFamilyIndices indices = retrieve_queue_family_indices(physical_device);

    VkPhysicalDeviceFeatures device_features{};

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> unique_queue_families = { indices.graphics_family.value(), indices.present_family.value(), };
    float priority = 1.0f;
    for (auto queue_family : unique_queue_families) {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = indices.graphics_family.value();
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &priority;
        queue_create_infos.push_back(queue_create_info);
    }

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = (uint32_t)queue_create_infos.size();
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.pEnabledFeatures = &device_features;
    device_create_info.enabledExtensionCount = device_extensions.size();
    device_create_info.ppEnabledExtensionNames = device_extensions.data();

    if (vkCreateDevice(physical_device, &device_create_info, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Unable to create logical device.");
    }
    vkGetDeviceQueue(device, indices.graphics_family.value(), 0, &graphics_queue);
    vkGetDeviceQueue(device, indices.present_family.value(), 0, &present_queue);
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
        extensions[i] = glfwExtensions[i];
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
    auto are_extensions_supported = are_device_extensions_supported(device);
    bool is_swapchain_supported = false;
    if (are_extensions_supported) {
        auto details = query_swapchain_support_details(device);
        is_swapchain_supported = !details.formats.empty() && !details.present_modes.empty();
    }

    return
        device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
        && queue_indicies.are_complete()
        && is_swapchain_supported;
}

Context::SwapChainSupportDetails Context::query_swapchain_support_details(VkPhysicalDevice device) {
    SwapChainSupportDetails details{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);
    if (format_count != 0) {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats.data());
    }

    uint32_t modes_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &modes_count, nullptr);
    if (modes_count != 0) {
        details.present_modes.resize(modes_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &modes_count, details.present_modes.data());
    }

    return details;
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
        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
        if (present_support) {
            indices.present_family = i;
        }
        if (indices.are_complete()) {
            break;
        }
    }

    return indices;
}

bool Context::are_device_extensions_supported(VkPhysicalDevice device) {
    uint32_t count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> extensions(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
    for (auto device_extension : device_extensions) {
        bool is_supported = false;
        for (auto extension : extensions) {
            if (strcmp(device_extension, extension.extensionName) == 0) {
                is_supported = true;
                break;
            }
        }
        if (!is_supported) {
            return false;
        }
    }

    return true;
}

VkPresentModeKHR Context::choose_present_mode(std::vector<VkPresentModeKHR>& modes) {
    for (auto& mode : modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkSurfaceFormatKHR Context::choose_surface_format(std::vector<VkSurfaceFormatKHR>& formats) {
    for (auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return formats[0];
}

VkExtent2D Context::choose_swapchain_extent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        throw std::runtime_error("TODO");
    }
}

void Context::create_swapchain() {
    auto details = query_swapchain_support_details(physical_device);
    auto extent = choose_swapchain_extent(details.capabilities);
    auto format = choose_surface_format(details.formats);
    auto present_mode = choose_present_mode(details.present_modes);

    uint32_t image_count = details.capabilities.minImageCount + 1;
    if (details.capabilities.maxImageCount > 0 && image_count > details.capabilities.maxImageCount) {
        image_count = details.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = surface;
    info.imageExtent = extent;
    info.imageFormat = format.format;
    info.imageColorSpace = format.colorSpace;
    info.presentMode = present_mode;
    info.minImageCount = image_count;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.preTransform = details.capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.clipped = VK_TRUE;
    info.oldSwapchain = VK_NULL_HANDLE;

    auto indicies = retrieve_queue_family_indices(physical_device);
    uint32_t queue_family_indicies[] = { indicies.graphics_family.value(), indicies.present_family.value(), };
    if (indicies.graphics_family != indicies.present_family) {
        info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = queue_family_indicies;
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.queueFamilyIndexCount = 0;
        info.pQueueFamilyIndices = nullptr;
    }

    if (vkCreateSwapchainKHR(device, &info, nullptr, &swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Unable to create swapchain.");
    }

    swapchain_extent = extent;
    swapchain_format = format.format;

    vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
    swapchain_images.resize(image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images.data());
}


void Context::create_image_views() {
    swapchain_image_views.resize(swapchain_images.size());
    for (size_t i = 0; i < swapchain_image_views.size(); i++) {
        VkImageViewCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.format = swapchain_format;
        info.image = swapchain_images[i];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device, &info, nullptr, &swapchain_image_views[i]) != VK_SUCCESS) {
            throw std::runtime_error("Unable to create swapchain image view");
        }
    }
}

}
