#include "context.hpp"
#include <cassert>
#include <optional>
#include "../common/hash_utils.hpp"

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

    std::array<const char *, 1> extensions = { "VK_KHR_swapchain", };

    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queue_info;
    info.enabledExtensionCount = (uint32_t)extensions.size();
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
    std::vector<VkImage> swapchain_vk_images(swapchain_image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, swapchain_vk_images.data());
    swapchain_image_views.resize(swapchain_image_count);
    for (uint32_t i = 0; i < swapchain_image_count; i++) {
        swapchain_images[i] = Image(swapchain_vk_images[i]);
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
        swapchain_image_views[i] = ImageView(image_view);
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

    VkDescriptorPoolSize pool_sizes[2];
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = 128;
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[1].descriptorCount = 128;
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = pool_sizes;
    pool_info.maxSets = 16;

    for (size_t i = 0; i < count; i++) {
        auto& frame_context = frame_contexts[i];
        vkCreateCommandPool(device, &command_pool_info, nullptr, &frame_context.command_pool);
        vkCreateFence(device, &fence_info, nullptr, &frame_context.render_fence);
        vkCreateSemaphore(device, &semaphore_info, nullptr, &frame_context.present_semaphore);
        vkCreateSemaphore(device, &semaphore_info, nullptr, &frame_context.render_semaphore);
        VkDescriptorPool pool;
        vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
        frame_context.descriptor_pools.push_back(DescriptorPool(device, pool, 16));
    }
}

void Context::begin_frame() {
    auto& frame_context = get_current_frame_context();
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, frame_context.present_semaphore, nullptr, &swapchain_image_index);
    vkWaitForFences(device, 1, &frame_context.render_fence, true, 1000000000);
    vkResetFences(device, 1, &frame_context.render_fence);
    vkResetCommandPool(device, frame_context.command_pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
    for (auto& descriptor_pool : frame_context.descriptor_pools) {
        descriptor_pool.reset();
    }
    frame_context.current_descriptor_pool_index = 0;
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

void Context::release_buffer_on_frame_begin(Buffer buffer) {
    get_current_frame_context().destructors.push_back([=] {
        vmaDestroyBuffer(allocator, buffer.get_buffer(), buffer.get_allocation());
    });
}

void Context::release_image_on_frame_begin(Image image) {
    get_current_frame_context().destructors.push_back([=] {
        vmaDestroyImage(allocator, image.get_image(), image.get_allocation());
    });
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

Framebuffer Context::acquire_framebuffer(const FramebufferInfo& info) {
    // Need abstraction over VkImage and VkImageView.
    // For now assume image_view is swapchain image view.
    VkImageView image_views[FramebufferInfo::max_attachment_count];
    for (uint32_t i = 0; i < info.attachment_count; i++) {
        image_views[i] = info.attachments[i].get_image_view();
    }

    VkFramebufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    create_info.attachmentCount = info.attachment_count;
    create_info.pAttachments = image_views;
    create_info.width = info.extent.width;
    create_info.height = info.extent.height;
    create_info.layers = 1;
    create_info.renderPass = info.layout.get_vulkan_handle();

    VkFramebuffer framebuffer;
    vkCreateFramebuffer(device, &create_info, nullptr, &framebuffer);

    get_current_frame_context().destructors.push_back([=] {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    });

    return Framebuffer(framebuffer);
}

ImageView Context::get_swapchain_image_view() const {
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

Pipeline Context::acquire_pipeline(PipelineState &pipeline_state, RenderPass& render_pass, uint32_t subpass) {
    std:size_t hash = 0;
    hash_combine(hash, pipeline_state, render_pass.get_render_pass_layout().get_vulkan_handle(), subpass);
    VkPipeline pipeline;
    if (pipeline_cache.try_get(hash, pipeline)) {
        return Pipeline(pipeline);
    }
    VkPipelineShaderStageCreateInfo stages[5];
    for (uint32_t i = 0; i < pipeline_state.get_shader_count(); i++) {
        auto shader = pipeline_state.get_shader(i);
        stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[i].pNext = nullptr;
        stages[i].flags = 0;
        stages[i].stage = Shader::shader_stage_to_vulkan(shader.get_stage());
        stages[i].module = shader.get_shader_module();
        stages[i].pName = "main"; // shader.get_entry_point().data();
        stages[i].pSpecializationInfo = nullptr;
    }

    VkVertexInputAttributeDescription attributes_descriptions[Limits::MAX_VERTEX_ATTRIBUTE_DESCRIPTION_COUNT];
    for (uint32_t i = 0; i < pipeline_state.get_attribute_description_count(); i++) {
        attributes_descriptions[i] = pipeline_state.get_vertex_attribute_description(i);
    }

    VkVertexInputBindingDescription binding_descriptions[Limits::MAX_VERTEX_INPUT_BINDING_COUNT];
    for (uint32_t i = 0; i < pipeline_state.get_vertex_binding_description_count(); i++) {
        binding_descriptions[i] = pipeline_state.get_vertex_binding_description(i);
    }


    VkPipelineVertexInputStateCreateInfo vertex_input_state{};
    vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state.vertexBindingDescriptionCount = pipeline_state.get_vertex_binding_description_count();
    vertex_input_state.pVertexBindingDescriptions = binding_descriptions;
    vertex_input_state.vertexAttributeDescriptionCount = pipeline_state.get_attribute_description_count();
    vertex_input_state.pVertexAttributeDescriptions = attributes_descriptions;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state{};
    input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state.pNext = nullptr;
    input_assembly_state.flags = 0;
    input_assembly_state.topology = pipeline_state.get_topology();
    input_assembly_state.primitiveRestartEnable = VK_FALSE;

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.flags = 0;
    dynamic_state.pNext = nullptr;
    dynamic_state.pDynamicStates = dynamic_states;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.pNext = nullptr;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = nullptr;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = nullptr;

    VkPipelineRasterizationStateCreateInfo rasterization_state{};
    rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state.pNext = nullptr;
    rasterization_state.flags = 0;
    rasterization_state.depthClampEnable = VK_FALSE;
    rasterization_state.rasterizerDiscardEnable = VK_FALSE;
    rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state.lineWidth = 1.0f;
    rasterization_state.cullMode = pipeline_state.get_cull_mode();
    rasterization_state.frontFace = pipeline_state.get_front_face();
    rasterization_state.depthBiasEnable = pipeline_state.get_depth_bias_enable();
    rasterization_state.depthBiasConstantFactor = pipeline_state.get_depth_bias_constant_factor();
    rasterization_state.depthBiasSlopeFactor = pipeline_state.get_depth_bias_slope_factor();

    VkPipelineMultisampleStateCreateInfo multisample_state{};
    multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state.pNext = nullptr;
    multisample_state.flags = 0;
    multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_state.sampleShadingEnable = VK_FALSE;

     VkPipelineDepthStencilStateCreateInfo depth_stencil_state = pipeline_state.get_depth_stencil_state();

    VkPipelineColorBlendAttachmentState color_blend_attachment_state = pipeline_state.get_blending_state();

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

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = pipeline_state.get_shader_count();
    pipeline_info.pStages = stages;
    pipeline_info.pVertexInputState = &vertex_input_state;
    pipeline_info.pInputAssemblyState = &input_assembly_state;
    pipeline_info.pTessellationState = nullptr;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterization_state;
    pipeline_info.pMultisampleState = &multisample_state;
    pipeline_info.pDepthStencilState = &depth_stencil_state;
    pipeline_info.pColorBlendState = &color_blend_state;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = pipeline_state.get_pipeline_layout().get_pipeline_layout();
    pipeline_info.renderPass = render_pass.get_render_pass_layout().get_vulkan_handle();
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = 0;

    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline);
    pipeline_cache.add(hash, pipeline);

    return Pipeline(pipeline);
}

Buffer Context::acquire_buffer(VkDeviceSize size, VkBufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage) {
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = buffer_usage;

    VmaAllocationCreateInfo allocation_create_info{};
    allocation_create_info.usage = memory_usage;

    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;

    vmaCreateBuffer(allocator, &info, &allocation_create_info, &buffer, &allocation, &allocation_info);

    return Buffer(this, buffer, allocation, allocation_info);
}

void Context::map_memory(VmaAllocation allocation, void **map) {
    auto res = vmaMapMemory(allocator, allocation, map);
}

void Context::unmap_memory(VmaAllocation allocation) {
    vmaUnmapMemory(allocator, allocation);
}

Buffer Context::acquire_staging_buffer(VkDeviceSize size, VkBufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage) {
    auto& frame_context = get_current_frame_context();
    auto buffer = acquire_buffer(size, buffer_usage, memory_usage);
    release_buffer_on_frame_begin(buffer);
    return buffer;
}

void Context::release_buffer(Buffer buffer) {
    release_buffer_on_frame_begin(buffer);
}

void Context::flush(CommandBuffer command_buffer) {
    auto handle = command_buffer.get_vulkan_handle();
    vkEndCommandBuffer(handle);
    VkSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &handle;

    VkFence fence;
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(device, &fence_info, nullptr, &fence);

    vkQueueSubmit(graphics_queue, 1, &info, fence);
    vkWaitForFences(device, 1, &fence, VK_TRUE, 1000000000);

    vkDestroyFence(device, fence, nullptr);
}

DescriptorSet Context::acquire_descriptor_set(DescriptorSetLayout descriptor_set_layout) {
    DescriptorSet set;
    auto& frame_context = get_current_frame_context();
    auto& descriptor_pool = frame_context.descriptor_pools[frame_context.current_descriptor_pool_index];
    if (descriptor_pool.try_allocate_set(descriptor_set_layout, set)) {
        return set;
    }
    VkDescriptorPoolSize pool_sizes[2];
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = 128;
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[1].descriptorCount = 128;
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = pool_sizes;
    pool_info.maxSets = 16;
    VkDescriptorPool vulkan_descriptor_pool;
    vkCreateDescriptorPool(device, &pool_info, nullptr, &vulkan_descriptor_pool);
    frame_context.descriptor_pools.push_back(DescriptorPool(device, vulkan_descriptor_pool, 16));
    frame_context.current_descriptor_pool_index++;
    return acquire_descriptor_set(descriptor_set_layout);
}

void Context::update_descriptor_set(DescriptorSet descriptor_set, ResourceSet resource_set) {
    auto descriptor_set_handle = descriptor_set.get_descriptor_set();
    VkWriteDescriptorSet writes[Limits::MAX_DESCRIPTOR_SET_BINDING_COUNT];
    uint32_t current_write = 0;
    for (uint32_t i = 0; i < Limits::MAX_DESCRIPTOR_SET_BINDING_COUNT; i++) {
        const auto& binding = resource_set.get_binding(i);
        auto& write = writes[current_write];
        write = {};
        switch (binding.get_resource_type())
        {
        case ResourceType::None:
            continue;
        case ResourceType::UniformBuffer:
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.pBufferInfo = binding.get_buffer_info();
            break;
        case ResourceType::CombinedImageSampler:
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo = binding.get_image_info();
            break;
        default:
            throw std::runtime_error("Not implemented exception.");
        }
        write.dstSet = descriptor_set_handle;
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstBinding = i;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;

        current_write++;
    }
    if (current_write != 0) {
        vkUpdateDescriptorSets(device, current_write, writes, 0, nullptr);
    }
}

PipelineLayout Context::acquire_pipeline_layout(ResourceSet sets[Limits::MAX_DESCRIPTOR_SET_COUNT]) {
    DescriptorSetLayout descriptor_set_layouts[Limits::MAX_DESCRIPTOR_SET_COUNT];
    VkDescriptorSetLayout descriptor_set_layout_handles[Limits::MAX_DESCRIPTOR_SET_COUNT];
    VkPipelineLayout pipeline_layout;
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = Limits::MAX_DESCRIPTOR_SET_COUNT;
    pipeline_layout_info.pSetLayouts = descriptor_set_layout_handles;
    size_t pipeline_layout_hash = 0;
    for (int i = 0; i < Limits::MAX_DESCRIPTOR_SET_COUNT; i++) {
        auto& set = sets[i];
        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        VkDescriptorSetLayoutBinding bindings[Limits::MAX_DESCRIPTOR_SET_BINDING_COUNT];
        layout_info.pBindings = bindings;
        layout_info.flags = 0;
        uint32_t current_binding = 0;
        size_t hash = 0;
        for (uint32_t j = 0; j < Limits::MAX_DESCRIPTOR_SET_BINDING_COUNT; j++) {
            auto binding = set.get_binding(j);
            switch (binding.get_resource_type())
            {
            case ResourceType::None:
                continue;
            case ResourceType::UniformBuffer:
                bindings[current_binding].descriptorCount = 1;
                bindings[current_binding].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                // TODO: Dehardcode.
                bindings[current_binding].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
                break;
            case ResourceType::CombinedImageSampler:
                bindings[current_binding].descriptorCount = 1;
                bindings[current_binding].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                // TODO: Dehardcode.
                bindings[current_binding].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
                break;
            default:
                break;
            }
            hash_combine(hash, j, binding.get_resource_type());
            bindings[current_binding].binding = j;
            bindings[current_binding].pImmutableSamplers = nullptr;
            current_binding++;
        }
        layout_info.bindingCount = current_binding;
        VkDescriptorSetLayout descriptor_set_layout;
        if (!descriptor_set_layout_cache.try_get(hash, descriptor_set_layout)) {
            vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &descriptor_set_layout);
            descriptor_set_layout_cache.add(hash, descriptor_set_layout);
        }
        hash_combine(pipeline_layout_hash, i, descriptor_set_layout);
        descriptor_set_layout_handles[i] = descriptor_set_layout;
        descriptor_set_layouts[i] = DescriptorSetLayout(descriptor_set_layout);
    }
    if (!pipeline_layout_cache.try_get(pipeline_layout_hash, pipeline_layout)) {
        vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout);
        pipeline_layout_cache.add(pipeline_layout_hash, pipeline_layout);
    }
    return PipelineLayout(pipeline_layout, descriptor_set_layouts);
}

Image Context::acquire_image(VkExtent3D extent, VkFormat format, VkImageUsageFlags image_usage, VmaMemoryUsage memory_usage) {
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.flags = 0;
    image_info.extent = extent;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = format;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.usage = image_usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocation_create_info{};
    allocation_create_info.usage = memory_usage;

    VkImage image;
    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;
    vmaCreateImage(allocator, &image_info, &allocation_create_info, &image, &allocation, &allocation_info);

    return Image(image, allocation, allocation_info);
}

Image Context::acquire_temporary_image(VkExtent3D extent, VkFormat format, VkImageUsageFlags image_usage, VmaMemoryUsage memory_usage) {
    auto image = acquire_image(extent, format, image_usage, memory_usage);
    release_image_on_frame_begin(image);
    return image;
}

ImageView Context::create_image_view(VkFormat format, Image& image, VkImageAspectFlags aspect) {
    VkImageViewCreateInfo image_view_info{};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.format = format;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.subresourceRange.aspectMask = aspect;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = 1;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = 1;
    image_view_info.image = image.get_image();

    VkImageView image_view;
    vkCreateImageView(device, &image_view_info, nullptr, &image_view);

    return ImageView(image_view);
}


ImageView Context::create_temporary_image_view(VkFormat format, Image& image, VkImageAspectFlags aspect) {
    auto image_view = create_image_view(format, image, aspect);
    get_current_frame_context().destructors.push_back([=]{
        destroy_image_view(image_view);
    });
    return image_view;
}

void Context::release_image(Image image) {
    release_image_on_frame_begin(image);
}

void Context::destroy_image_view(ImageView image_view) {
    vkDestroyImageView(device, image_view.get_image_view(), nullptr);
}

Sampler Context::acquire_sampler(
    VkSamplerAddressMode address_mode,
    VkFilter filter,
    VkBool32 compare_enable,
    VkCompareOp compare_op
) {
    return acquire_sampler(address_mode, address_mode, address_mode, filter, filter, compare_enable, compare_op);
}

Sampler Context::acquire_sampler(
    VkSamplerAddressMode address_mode_u,
    VkSamplerAddressMode address_mode_v,
    VkSamplerAddressMode address_mode_w,
    VkFilter min_filter,
    VkFilter mag_filter,
    VkBool32 compare_enable,
    VkCompareOp compare_op
) {
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.addressModeU = address_mode_u;
    sampler_info.addressModeV = address_mode_v;
    sampler_info.addressModeW = address_mode_w;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.minFilter = min_filter;
    sampler_info.magFilter = mag_filter;
    sampler_info.compareEnable = compare_enable;
    sampler_info.compareOp = compare_op;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.maxLod = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.unnormalizedCoordinates = VK_FALSE;

    VkSampler sampler;
    vkCreateSampler(device, &sampler_info, nullptr, &sampler);

    return Sampler(sampler);
}

VkFormat Context::get_swapchain_format() const {
    return swapchain_format;
}

VkRenderPass Context::create_render_pass(const RenderPassInfo& info) {
    assert(info.attachent_count == info.layout.get_info().attachent_count);
    VkRenderPassCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    VkAttachmentDescription attachment_descriptions[RenderPassInfo::max_attachment_count];
    VkAttachmentReference references[RenderPassInfo::max_attachment_count];
    uint32_t reference_index = 0;
    uint32_t preserve_indicies[RenderPassInfo::max_attachment_count];
    uint32_t preserve_index = 0;
    create_info.attachmentCount = info.attachent_count;
    create_info.pAttachments = attachment_descriptions;
    const auto& layout_info = info.layout.get_info();
    for (uint32_t i = 0; i < info.attachent_count; i++) {
        auto& desc = attachment_descriptions[i];
        const auto& attachment_info = info.attachments[i];
        const auto& layout_attachment_info = layout_info.attachments[i];
        desc.flags = 0;
        desc.format = layout_attachment_info.format;
        desc.samples = VK_SAMPLE_COUNT_1_BIT;
        desc.loadOp = attachment_info.load_op;
        desc.storeOp = attachment_info.store_op;
        desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        desc.finalLayout = attachment_info.final_layout;
    }
    VkSubpassDescription subpass{};
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;
    const auto& subpass_info = layout_info.subpass;
    subpass.flags = 0;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = nullptr;
    subpass.colorAttachmentCount = subpass_info.color_attachment_count;
    subpass.pColorAttachments = &references[reference_index];
    for (uint32_t j = 0; j < subpass_info.color_attachment_count; j++) {
        auto attachment_index = subpass_info.color_attachments[j];
        auto& attachment_info = info.attachments[attachment_index];
        auto& reference = references[reference_index++];
        reference.attachment = attachment_index;
        reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    subpass.pResolveAttachments = nullptr;
    subpass.pDepthStencilAttachment = nullptr;
    if (subpass_info.depth_attachment.has_value()) {
        subpass.pDepthStencilAttachment = &references[reference_index];
        auto& reference = references[reference_index++];
        reference.attachment = subpass_info.depth_attachment.value();
        // TODO: How to determine if it's readonly?
        reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    subpass.preserveAttachmentCount = info.attachent_count
        - subpass_info.color_attachment_count
        - (subpass_info.depth_attachment.has_value() ? 1 : 0);
    if (subpass.preserveAttachmentCount != 0) {
        subpass.pPreserveAttachments = &preserve_indicies[preserve_index];
        auto first_preserve_index = preserve_index;
        const uint32_t* color_attachments = subpass_info.color_attachments;
        const uint32_t* color_attachments_end = color_attachments + subpass_info.color_attachment_count;

        for (uint32_t i = 0; i < info.attachent_count; i++) {
            if (
                i != subpass_info.depth_attachment
                && std::find(color_attachments, color_attachments_end, i) == color_attachments_end
            ) {
                preserve_indicies[preserve_index++] = i;
            }
        }
        assert(preserve_index - first_preserve_index == subpass.preserveAttachmentCount);
    }
    VkRenderPass render_pass;
    VK_CHECK(
        vkCreateRenderPass(device, &create_info, nullptr, &render_pass),
        "Unable to create render pass."
    );
    return render_pass;
}

RenderPassLayout Context::acquire_render_pass_layout(const RenderPassLayoutInfo& info) {
    RenderPassInfo render_pass_info;
    render_pass_info.attachent_count = info.attachent_count;
    for (uint32_t i = 0; i < info.attachent_count; i++) {
        auto& attachment = render_pass_info.attachments[i];
        // final_layout is not a part of Render Pass compatibility,
        // but still have to fill it in some way.
        // Otherwise validation layers will give erros.
        switch (info.attachments[i].format)
        {
        case VK_FORMAT_D32_SFLOAT:
            attachment.final_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            break;
        default:
            attachment.final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;
        }
        attachment.load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
    render_pass_info.layout = RenderPassLayout(info);
    size_t hash = 0;
    VkRenderPass render_pass;
    hash_combine(hash, info);
    if (!render_pass_cache.try_get(hash, render_pass)) {
        render_pass = create_render_pass(render_pass_info);
    }
    return RenderPassLayout(info, render_pass);
}

RenderPass Context::acquire_render_pass(const RenderPassInfo& info) {
    size_t hash = 0;
    VkRenderPass render_pass;
    if (!render_pass_cache.try_get(hash, render_pass)) {
        render_pass = create_render_pass(info);
    }
    return RenderPass(render_pass, info.layout);
}

}
