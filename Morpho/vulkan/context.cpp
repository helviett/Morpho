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
    create_render_pass();
    create_graphics_pipeline();
    create_command_pool();
    create_command_buffers();
    create_sync_structures();
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
    vkDestroyCommandPool(device, command_pool, nullptr);
    for (auto framebuffer : framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    vkDestroyRenderPass(device, render_pass, nullptr);

    for (auto image_view : swapchain_image_views) {
        vkDestroyImageView(device, image_view, nullptr);
    }

    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroyDevice(device, nullptr);

    if (enable_validation_layers) {
        DestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
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

std::vector<char> Context::read_all_bytes(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Can't open file '" + filename + "'");
    }

    auto size = (size_t)file.tellg();
    std::vector<char> bytes(size);
    file.seekg(0);
    file.read(bytes.data(), size);
    file.close();

    return bytes;
}

VkShaderModule Context::create_shader_module(const std::vector<char>& bytes) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pCode = reinterpret_cast<const uint32_t*>(bytes.data());
    info.codeSize = bytes.size();
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &info, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Unable to create shader module.");
    }

    return shaderModule;
}

void Context::create_graphics_pipeline() {
    auto vertShader = create_shader_module(read_all_bytes("assets/shaders/triangle.vert.spv"));
    auto fragShader = create_shader_module(read_all_bytes("assets/shaders/triangle.frag.spv"));

    VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo{};
    vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderStageCreateInfo.module = vertShader;
    vertexShaderStageCreateInfo.pName = "main";
    VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo{};
    fragShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageCreateInfo.module = fragShader;
    fragShaderStageCreateInfo.pName = "main";
    VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderStageCreateInfo, fragShaderStageCreateInfo, };

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.pVertexBindingDescriptions = nullptr;
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    vertex_input_info.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapchain_extent.width;
    viewport.height = (float)swapchain_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = swapchain_extent;
    scissor.offset = { 0, 0 };

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.pAttachments = &color_blend_attachment;
    color_blending.blendConstants[0] = 0.0f;
    color_blending.blendConstants[1] = 0.0f;
    color_blending.blendConstants[2] = 0.0f;
    color_blending.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 0;
    pipeline_layout_info.pSetLayouts = nullptr;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
        throw std::runtime_error("Unable to create pipeline layout.");
    }

    VkGraphicsPipelineCreateInfo pipeline_create_info{};
    pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_create_info.stageCount = 2;
    pipeline_create_info.pStages = shaderStages;
    pipeline_create_info.pVertexInputState = &vertex_input_info;
    pipeline_create_info.pInputAssemblyState = &input_assembly;
    pipeline_create_info.pViewportState = &viewport_state;
    pipeline_create_info.pRasterizationState = &rasterizer;
    pipeline_create_info.pMultisampleState = &multisampling;
    pipeline_create_info.pDepthStencilState = nullptr;
    pipeline_create_info.pColorBlendState = &color_blending;
    pipeline_create_info.pDynamicState = nullptr;
    pipeline_create_info.layout = pipeline_layout;
    pipeline_create_info.renderPass = render_pass;
    pipeline_create_info.subpass = 0;
    pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_create_info.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Unable to create graphics pipeline.");
    }
}

void Context::create_render_pass() {
    VkAttachmentDescription color_attachment{};
    color_attachment.format = swapchain_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkRenderPassCreateInfo rende_pass_create_info{};
    rende_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rende_pass_create_info.attachmentCount = 1;
    rende_pass_create_info.pAttachments = &color_attachment;
    rende_pass_create_info.subpassCount = 1;
    rende_pass_create_info.pSubpasses = &subpass;
    if (vkCreateRenderPass(device, &rende_pass_create_info, nullptr, &render_pass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void Context::create_framebuffers() {
    framebuffers.resize(swapchain_image_views.size());
    for (size_t i = 0; i < framebuffers.size(); i++) {
        VkImageView attachments[] = {
            swapchain_image_views[i],
        };

        VkFramebufferCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        create_info.renderPass = render_pass;
        create_info.attachmentCount = 1;
        create_info.pAttachments = attachments;
        create_info.width = swapchain_extent.width;
        create_info.height = swapchain_extent.height;
        create_info.layers = 0;
        if (vkCreateFramebuffer(device, &create_info, nullptr, &framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Unable to create framebuffer.");
        }
    }
}

void Context::create_command_pool() {
    auto indicies = retrieve_queue_family_indices(physical_device);

    VkCommandPoolCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_info.queueFamilyIndex = indicies.graphics_family.value();
    create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(device, &create_info, nullptr, &command_pool) != VK_SUCCESS) {
        throw std::runtime_error("Unable to create command pool.");
    }
}

void Context::create_command_buffers() {
    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = command_pool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device, &info, &command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Unable to allocate command buffer.");
    }
}

void Context::create_sync_structures() {
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.pNext = nullptr;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.pNext = nullptr;
    semaphore_info.flags = 0;

    if (
        vkCreateFence(device, &fence_info, nullptr, &render_fence) != VK_SUCCESS
        || vkCreateSemaphore(device, &semaphore_info, nullptr, &render_semaphore) != VK_SUCCESS
        || vkCreateSemaphore(device, &semaphore_info, nullptr, &present_semaphore) != VK_SUCCESS
    ) {
        throw std::runtime_error("Unable to create synchronization structures.");
    }
}

}
