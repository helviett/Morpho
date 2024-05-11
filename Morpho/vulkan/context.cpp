#include "context.hpp"
#include <cassert>
#include <optional>
#include <cstddef>
#include <vulkan/vulkan_core.h>
#include "resource_manager.hpp"

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
    query_gpu_properties();

    VK_CHECK(try_create_device(), "Unable to create logical device.")
    retrieve_queues();

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_1;
    allocatorInfo.physicalDevice = gpu;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;

    vmaCreateAllocator(&allocatorInfo, &allocator);

    ResourceManager::create(this);

    create_swapchain();

}

VKAPI_ATTR VkBool32 VKAPI_CALL Context::debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
) {
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    } else {
        std::cout << "validation layer: " << pCallbackData->pMessage << std::endl;
    }
    return VK_FALSE;
}

VkPhysicalDevice Context::select_gpu() {
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
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(best_candidate, &properties);
    std::cout << "Selected: " << properties.deviceName << std::endl;
    return best_candidate;
}

void Context::query_gpu_properties() {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(gpu, &properties);
    min_uniform_buffer_offset_alignment = properties.limits.minUniformBufferOffsetAlignment;
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
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
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

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info{};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueCount = 1;
    queue_info.queueFamilyIndex = graphics_queue_family_index;
    queue_info.pQueuePriorities = &priority;

    std::array<const char *, 2> extensions = { "VK_KHR_swapchain", VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME, };

    VkPhysicalDeviceFeatures2 features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, };
    vkGetPhysicalDeviceFeatures2(gpu, &features);

    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queue_info;
    info.enabledExtensionCount = (uint32_t)extensions.size();
    info.ppEnabledExtensionNames = extensions.data();
    info.pNext = &features;


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
    assert(swapchain_image_count <= 16);
    Texture swapchain_textures[16];
    swapchain_texture_handles.resize(swapchain_image_count);
    std::vector<VkImage> swapchain_vk_images(swapchain_image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, swapchain_vk_images.data());
    swapchain_texture_handles.resize(swapchain_image_count);
    ResourceManager* rm = ResourceManager::get();
    for (uint32_t i = 0; i < swapchain_image_count; i++) {
        swapchain_textures[i].image = swapchain_vk_images[i];
        swapchain_textures[i].format = swapchain_format;
        VkImageViewCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.format = swapchain_format;
        info.image = swapchain_vk_images[i];
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
        VkImageView image_view;
        vkCreateImageView(device, &info, nullptr, &image_view);
        swapchain_textures[i].image_view = image_view;
        swapchain_textures[i].aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        swapchain_texture_handles[i] = rm->register_texture(swapchain_textures[i]);
    }
}

void Context::set_frame_context_count(uint32_t count) {
    frame_context_count = count;
    frame_context_index = 0;

    VkCommandPoolCreateInfo command_pool_info{};
    command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_info.queueFamilyIndex = graphics_queue_family_index;
    command_pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.flags = 0;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < count; i++) {
        auto& frame_context = frame_contexts[i];
        vkCreateCommandPool(device, &command_pool_info, nullptr, &frame_context.command_pool);
        vkCreateFence(device, &fence_info, nullptr, &frame_context.render_finished_fence);
        vkCreateSemaphore(device, &semaphore_info, nullptr, &frame_context.image_ready_semaphore);
        vkCreateSemaphore(device, &semaphore_info, nullptr, &frame_context.render_semaphore);
    }
}

void Context::begin_frame() {
    auto& frame_context = get_current_frame_context();
    VK_CHECK(vkWaitForFences(device, 1, &frame_context.render_finished_fence, VK_TRUE, 10000000000), __FUNCTION__);
    vkResetFences(device, 1, &frame_context.render_finished_fence);
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, frame_context.image_ready_semaphore, VK_NULL_HANDLE, &swapchain_image_index);
    vkResetCommandPool(device, frame_context.command_pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
    for (auto it = frame_context.destructors.rbegin(); it != frame_context.destructors.rend(); it++) {
        (*it)();
    }
    frame_context.destructors.clear();
}

void Context::end_frame() {
    VkPresentInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.pImageIndices = &swapchain_image_index;
    info.pSwapchains = &swapchain;
    info.swapchainCount = 1;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &get_current_frame_context().render_semaphore;


    vkQueuePresentKHR(graphics_queue, &info);
    frame_context_index = (frame_context_index + 1) % frame_context_count;
}

CommandBuffer Context::acquire_command_buffer() {
    VkCommandBuffer command_buffer;
    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = get_current_frame_context().command_pool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;

    vkAllocateCommandBuffers(device, &info, &command_buffer);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &begin_info);

    CommandBuffer cmd(command_buffer);

    return cmd;
}


Context::FrameContext& Context::get_current_frame_context() {
    return frame_contexts[frame_context_index];
}

void Context::release_buffer_on_frame_begin(Buffer buffer) {
    get_current_frame_context().destructors.push_back([=, this] {
        vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
    });
}

void Context::release_texture_on_frame_begin(Texture texture) {
    get_current_frame_context().destructors.push_back([=, this] {
        if (texture.owns_image)
        {
            vmaDestroyImage(allocator, texture.image, texture.allocation);
        }
        vkDestroyImageView(device, texture.image_view, nullptr);
    });
}

void Context::submit(CommandBuffer command_buffer) {
    auto handle = command_buffer.get_vulkan_handle();
    vkEndCommandBuffer(handle);
    auto& frame_context = get_current_frame_context();
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, };
    VkSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &handle;
    info.pWaitSemaphores = &frame_context.image_ready_semaphore;
    info.waitSemaphoreCount = 1;
    info.pWaitDstStageMask = wait_stages;
    info.pSignalSemaphores = &frame_context.render_semaphore;
    info.signalSemaphoreCount = 1;

    vkQueueSubmit(graphics_queue, 1, &info, frame_context.render_finished_fence);
}

Framebuffer Context::acquire_framebuffer(const FramebufferInfo& info) {
    // Need abstraction over VkImage and VkImageView.
    // For now assume image_view is swapchain image view.
    VkImageView image_views[FramebufferInfo::max_attachment_count];
    for (uint32_t i = 0; i < info.attachment_count; i++) {
        image_views[i] = ResourceManager::get()->get_texture(info.attachments[i]).image_view;
    }

    VkFramebufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    create_info.attachmentCount = info.attachment_count;
    create_info.pAttachments = image_views;
    create_info.width = info.extent.width;
    create_info.height = info.extent.height;
    create_info.layers = 1;
    create_info.renderPass = ResourceManager::get()->get_render_pass_layout(info.layout).render_pass;

    VkFramebuffer vk_framebuffer;
    vkCreateFramebuffer(device, &create_info, nullptr, &vk_framebuffer);

    get_current_frame_context().destructors.push_back([=, this] {
        vkDestroyFramebuffer(device, vk_framebuffer, nullptr);
    });

    Framebuffer framebuffer{};
    framebuffer.framebuffer = vk_framebuffer;
    return framebuffer;
}

Handle<Texture> Context::get_swapchain_texture() const {
    return swapchain_texture_handles[swapchain_image_index];
}

VkExtent2D Context::get_swapchain_extent() const {
    return swapchain_extent;
}

uint64_t Context::get_uniform_buffer_alignment() const {
    return min_uniform_buffer_offset_alignment;
}

VkFormat Context::get_swapchain_format() const {
    return swapchain_format;
}

void Context::wait_queue_idle() {
    vkQueueWaitIdle(graphics_queue);
}

void Context::create_cmd_pool(CmdPool** out_pool) {
    CmdPool* pool = (CmdPool*)malloc(sizeof(CmdPool));
    pool->current_frame = 0;
    pool->device = device;
    VkCommandPoolCreateInfo command_pool_info{};
    command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_info.queueFamilyIndex = graphics_queue_family_index;
    command_pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    for (uint32_t i = 0; i < MAX_FRAME_CONTEXTS; i++) {
        VkResult result = vkCreateCommandPool(device, &command_pool_info, nullptr, &pool->cmd_pools[i]);
        VK_CHECK(result, "Unable to create VkCommandPool.");
    }
    *out_pool = pool;
}

void Context::destroy_cmd_pool(CmdPool* pool) {
    for (uint32_t i = 0; i < MAX_FRAME_CONTEXTS; i++) {
        vkDestroyCommandPool(device, pool->cmd_pools[i], nullptr);
    }
    free(pool);
}


CommandBuffer CmdPool::allocate() {
    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = cmd_pools[current_frame];
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;
    VkCommandBuffer vk_cmd;
    vkAllocateCommandBuffers(device, &info, &vk_cmd);
    VkCommandBufferBeginInfo begin_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(vk_cmd, &begin_info);
    return CommandBuffer(vk_cmd);
}

void CmdPool::next_frame() {
    current_frame = (current_frame + 1) % MAX_FRAME_CONTEXTS;
    vkResetCommandPool(device, cmd_pools[current_frame], VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
}

}
