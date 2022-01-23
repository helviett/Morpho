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

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_1;
    allocatorInfo.physicalDevice = gpu;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;

    VmaAllocator allocator;
    vmaCreateAllocator(&allocatorInfo, &allocator);
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

void Context::set_frame_context_count(uint32_t count) {
    frame_context_count = count;
    frame_context_index = 0;

    VkCommandPoolCreateInfo command_pool_info{};
    command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_info.queueFamilyIndex = graphics_queue_family_index;
    command_pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < count; i++) {
        auto& frame_context = frame_contexts[i];
        vkCreateCommandPool(device, &command_pool_info, nullptr, &frame_context.command_pool);
        vkCreateFence(device, &fence_info, nullptr, &frame_context.render_fence);
        vkCreateSemaphore(device, &semaphore_info, nullptr, &frame_context.present_semaphore);
        vkCreateSemaphore(device, &semaphore_info, nullptr, &frame_context.render_semaphore);
    }
}

void Context::begin_frame() {
    auto& frame_context = get_current_frame_context();
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, frame_context.present_semaphore, nullptr, &swapchain_image_index);
    vkWaitForFences(device, 1, &frame_context.render_fence, true, 1000000000);
    vkResetFences(device, 1, &frame_context.render_fence);
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

    CommandBuffer cmd(command_buffer, this);

    return cmd;
}


Context::FrameContext& Context::get_current_frame_context() {
    return frame_contexts[frame_context_index];
}


void Context::submit(CommandBuffer command_buffer) {
    auto handle = command_buffer.get_vulkan_handle();
    vkEndCommandBuffer(handle);
    auto& frame_context = get_current_frame_context();
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, };
    VkSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &handle;
    info.pWaitSemaphores = &frame_context.present_semaphore;
    info.waitSemaphoreCount = 1;
    info.pWaitDstStageMask = wait_stages;
    info.pSignalSemaphores = &frame_context.render_semaphore;
    info.signalSemaphoreCount = 1;

    vkQueueSubmit(graphics_queue, 1, &info, frame_context.render_fence);
}

RenderPass Context::acquire_render_pass(RenderPassInfo& render_pass_info) {
    VkAttachmentDescription desc{};
    desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    desc.format = swapchain_format;
    desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    desc.samples = VK_SAMPLE_COUNT_1_BIT;

    VkAttachmentReference ref{};
    ref.attachment = 0;
    ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = 1;
    create_info.pAttachments = &desc;
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;
    create_info.dependencyCount = 1;
    create_info.pDependencies = &dependency;

    VkRenderPass render_pass;

    VK_CHECK(vkCreateRenderPass(device, &create_info, nullptr, &render_pass), "Unable to create render pass.")

    get_current_frame_context().destructors.push_back([=] {
        vkDestroyRenderPass(device, render_pass, nullptr);
    });

    return RenderPass(render_pass);
}

Framebuffer Context::acquire_framebuffer(RenderPass render_pass, RenderPassInfo& render_pass_info) {
    // Need abstraction over VkImage and VkImageView.
    // For now assume image_view is swapchain image view.
    VkFramebufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &render_pass_info.image_view;
    info.width = swapchain_extent.width;
    info.height = swapchain_extent.height;
    info.layers = 1;
    info.renderPass = render_pass.get_vulkan_handle();

    VkFramebuffer framebuffer;
    vkCreateFramebuffer(device, &info, nullptr, &framebuffer);

    get_current_frame_context().destructors.push_back([=] {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    });

    return Framebuffer(framebuffer);
}

VkImageView Context::get_swapchain_image_view() const {
    return swapchain_image_views[swapchain_image_index];
}

Shader Context::acquire_shader(char* data, uint32_t size) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = size;
    info.pCode = reinterpret_cast<const uint32_t*>(data);

    VkShaderModule shader_module;
    VK_CHECK(vkCreateShaderModule(device, &info, nullptr, &shader_module), "Unable to create shader module.")

    return Shader(shader_module);
}

VkExtent2D Context::get_swapchain_extent() const {
    return swapchain_extent;
}

Pipeline Context::acquire_pipeline(PipelineInfo &info, RenderPass& render_pass, uint32_t subpass) {

    VkPipelineShaderStageCreateInfo stages[5];
    for (uint32_t i = 0; i < info.get_shader_count(); i++) {
        auto shader = info.get_shader(i);
        stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[i].pNext = nullptr;
        stages[i].flags = 0;
        stages[i].stage = Shader::shader_stage_to_vulkan(shader.get_stage());
        stages[i].module = shader.get_shader_module();
        stages[i].pName = "main"; // shader.get_entry_point().data();
        stages[i].pSpecializationInfo = nullptr;
    }

    VkPipelineVertexInputStateCreateInfo vertex_input_state{};
    vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state{};
    input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state.pNext = nullptr;
    input_assembly_state.flags = 0;
    input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_state.primitiveRestartEnable = VK_FALSE;

    // VkPipelineTessellationStateCreateInfo tesselation_state{};
    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)swapchain_extent.width;
    viewport.height = (float)swapchain_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain_extent;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.pNext = nullptr;
    viewport_state.flags = 0;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterization_state{};
    rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state.pNext = nullptr;
    rasterization_state.flags = 0;
    rasterization_state.depthClampEnable = VK_FALSE;
    rasterization_state.rasterizerDiscardEnable = VK_FALSE;
    rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state.lineWidth = 1.0f;
    rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization_state.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisample_state{};
    multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state.pNext = nullptr;
    multisample_state.flags = 0;
    multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_state.sampleShadingEnable = VK_FALSE;

    // VkPipelineDepthStencilStateCreateInfo depth_stencil_state{};

    // VkPipelineDynamicStateCreateInfo dynamic_state{};

    VkPipelineColorBlendAttachmentState color_blend_attachment_state{};
    color_blend_attachment_state.blendEnable = VK_FALSE;
    color_blend_attachment_state.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blend_state{};
    color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state.pNext = nullptr;
    color_blend_state.flags = 0;
    color_blend_state.logicOpEnable = VK_FALSE;
    color_blend_state.logicOp = VK_LOGIC_OP_COPY;
    color_blend_state.attachmentCount = 1;
    color_blend_state.pAttachments = &color_blend_attachment_state;
    color_blend_state.blendConstants[0] = 0.0f;
    color_blend_state.blendConstants[1] = 0.0f;
    color_blend_state.blendConstants[2] = 0.0f;
    color_blend_state.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.pNext = nullptr;
    layout_info.flags = 0;
    layout_info.setLayoutCount = 0;
    layout_info.pSetLayouts = nullptr;
    layout_info.pushConstantRangeCount = 0;
    layout_info.pPushConstantRanges = nullptr;

    VkPipelineLayout layout;
    vkCreatePipelineLayout(device, &layout_info, nullptr, &layout);


    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = info.get_shader_count();
    pipeline_info.pStages = stages;
    pipeline_info.pVertexInputState = &vertex_input_state;
    pipeline_info.pInputAssemblyState = &input_assembly_state;
    pipeline_info.pTessellationState = nullptr;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterization_state;
    pipeline_info.pMultisampleState = &multisample_state;
    pipeline_info.pDepthStencilState = nullptr;
    pipeline_info.pColorBlendState = &color_blend_state;
    pipeline_info.pDynamicState = nullptr;
    pipeline_info.layout = layout;
    pipeline_info.renderPass = render_pass.get_vulkan_handle();
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = 0;

    VkPipeline pipeline;
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline);

    auto& current_frame_context = get_current_frame_context();
    current_frame_context.destructors.push_back([=] {
        vkDestroyPipeline(device, pipeline, nullptr);
    });
    current_frame_context.destructors.push_back([=] {
        vkDestroyPipelineLayout(device, layout, nullptr);
    });

    return Pipeline(pipeline);
}

}
