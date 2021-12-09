#include "context.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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
    this->window = window;
    uint32_t wsi_extension_count;
    auto wsi_extensions = glfwGetRequiredInstanceExtensions(&wsi_extension_count);
    std::vector<const char*> extensions(wsi_extensions, wsi_extensions + wsi_extension_count);

    std::vector<const char*> layers;
    if (enable_validation_layers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    VK_CHECK(try_create_instance(extensions, layers), "Unable to create instance.")


    create_surface();

    if (enable_validation_layers) {
        auto messenger_info = get_default_messenger_create_info();
        VK_CHECK(
            CreateDebugUtilsMessengerEXT(instance, &messenger_info, nullptr, &debug_messenger),
            "Unable to create debug messenger."
        )
    }

    if ((gpu = select_gpu()) == VK_NULL_HANDLE) {
        throw std::runtime_error("There is no suitable gpu.");
    }

    VK_CHECK(try_create_device(), "Unable to create logical device.")
    retrieve_queues();
    create_swapchain();
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

VkPhysicalDevice Context::select_gpu()
{
    uint32_t gpu_count;
    vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr);
    std::vector<VkPhysicalDevice> gpus(gpu_count);
    vkEnumeratePhysicalDevices(instance, &gpu_count, gpus.data());
    uint32_t best_score = 0;
    VkPhysicalDevice best_candidate = VK_NULL_HANDLE;
    // TODO: Check for required device extensions support.
    for (auto candidate : gpus) {
        auto score = score_gpu(candidate);
        if (score > best_score) {
            best_score = score;
            best_candidate = candidate;
        }
    }

    return best_candidate;
}

VkResult Context::try_create_instance(std::vector<const char*>& extensions, std::vector<const char*>& layers) {
    VkApplicationInfo app_info{};
    app_info.pApplicationName = "Morpho sandbox";
    app_info.pEngineName = "NoName";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 0);
    app_info.engineVersion = VK_MAKE_VERSION(0, 0, 0);
    app_info.apiVersion = VK_MAKE_VERSION(1, 1, 0);

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.enabledExtensionCount = (uint32_t)extensions.size();
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledLayerCount = (uint32_t)layers.size();
    create_info.ppEnabledLayerNames = layers.data();
    create_info.pApplicationInfo = &app_info;

    VkDebugUtilsMessengerCreateInfoEXT messenger;

    if (enable_validation_layers) {
        messenger = get_default_messenger_create_info();
        create_info.pNext = &messenger;
    }

    return vkCreateInstance(&create_info, nullptr, &instance);
}

VkDebugUtilsMessengerCreateInfoEXT Context::get_default_messenger_create_info() {
    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debug_callback;

    return info;
}

uint32_t Context::score_gpu(VkPhysicalDevice gpu) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(gpu, &properties);
    switch (properties.deviceType)
    {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return 4;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return 3;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return 2;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
        return 1;
        break;
    default:
        return 0;
    }
}


VkResult Context::try_create_device() {
    uint32_t queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_family_count, queue_families.data());
    auto required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT;
    for (uint32_t i = 0; i < queue_families.size(); i++) {
        auto properties = queue_families[i];
        VkBool32 is_present_supported = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &is_present_supported);
        if ((properties.queueFlags & required) == required && is_present_supported) {
            graphics_queue_family_index = i;
            break;
        }
    }

    VkDeviceQueueCreateInfo queue_info{};
    queue_info.queueCount = 1;
    queue_info.queueFamilyIndex = graphics_queue_family_index;

    std::array<const char *, 1> extensions = { "VK_KHR_swapchain", };

    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queue_info;
    info.enabledExtensionCount = extensions.size();
    info.ppEnabledExtensionNames = extensions.data();


    return vkCreateDevice(gpu, &info, nullptr, &device);
}


void Context::retrieve_queues() {
    vkGetDeviceQueue(device, graphics_queue_family_index, 0, &graphics_queue);
}

void Context::create_surface() {
    VK_CHECK(
        glfwCreateWindowSurface(instance, window, nullptr, &surface),
        "Unable to create window surface."
    )
}


void Context::create_swapchain() {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &capabilities);

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &format_count, formats.data());
    auto chosen_format = formats[0];
    for (auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen_format = format;
        }
    }

    uint32_t modes_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &modes_count, nullptr);
    std::vector<VkPresentModeKHR> present_modes(modes_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &modes_count, present_modes.data());

    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    swapchain_extent = capabilities.currentExtent.width != UINT32_MAX
        ? capabilities.currentExtent
        : throw std::runtime_error("Unimplemented.");
    swapchain_format = chosen_format.format;

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = surface;
    info.imageExtent = swapchain_extent;
    info.imageFormat = swapchain_format;
    info.imageColorSpace = chosen_format.colorSpace;
    info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    info.minImageCount = image_count;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.preTransform = capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.clipped = VK_TRUE;
    info.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(device, &info, nullptr, &swapchain), "Unable to create swapchain.")

    uint32_t swapchain_image_count;
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, nullptr);
    swapchain_images.resize(swapchain_image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, swapchain_images.data());
    swapchain_image_views.resize(swapchain_image_count);
    for (int i = 0; i < swapchain_image_count; i++) {
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
        vkCreateImageView(device, &info, nullptr, &swapchain_image_views[i]);
    }
}

}
