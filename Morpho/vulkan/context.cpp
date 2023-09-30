#include "context.hpp"
#include <cassert>
#include <optional>
#include "../common/hash_utils.hpp"
#include <cstddef>
#include <vulkan/vulkan_core.h>

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
    create_swapchain();

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_1;
    allocatorInfo.physicalDevice = gpu;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;

    vmaCreateAllocator(&allocatorInfo, &allocator);

    VkDescriptorSetLayoutCreateInfo vk_descriptor_set_layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, };
    VK_CHECK(
        vkCreateDescriptorSetLayout(device, &vk_descriptor_set_layout_info, nullptr, &empty_descriptor_set_layout),
        "Unable to create empty VkDescriptorSetLayout"
    );
    VkDescriptorPoolCreateInfo vk_descriptor_pool_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, };
    vk_descriptor_pool_info.maxSets = 1;
    VK_CHECK(
        vkCreateDescriptorPool(device, &vk_descriptor_pool_info, nullptr, &empty_descriptor_pool),
        "Unable to create empty VkDescriptorPool"
    );
    VkDescriptorSetAllocateInfo vk_descriptor_set_allocate_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, };
    vk_descriptor_set_allocate_info.pSetLayouts = &empty_descriptor_set_layout;
    vk_descriptor_set_allocate_info.descriptorSetCount = 1;
    vk_descriptor_set_allocate_info.descriptorPool = empty_descriptor_pool;
    VK_CHECK(
        vkAllocateDescriptorSets(device, &vk_descriptor_set_allocate_info, &empty_descriptor_set),
        "Unable to allocate empty VkDescriptorSet"
    );
}

VKAPI_ATTR VkBool32 VKAPI_CALL Context::debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
) {
    std::cout << "validation layer: " << pCallbackData->pMessage << std::endl;
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
    swapchain_textures.resize(swapchain_image_count);
    std::vector<VkImage> swapchain_vk_images(swapchain_image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, swapchain_vk_images.data());
    swapchain_textures.resize(swapchain_image_count);
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
        vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
    });
}

void Context::release_texture_on_frame_begin(Texture texture) {
    get_current_frame_context().destructors.push_back([=] {
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
        image_views[i] = info.attachments[i].image_view;
    }

    VkFramebufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    create_info.attachmentCount = info.attachment_count;
    create_info.pAttachments = image_views;
    create_info.width = info.extent.width;
    create_info.height = info.extent.height;
    create_info.layers = 1;
    create_info.renderPass = info.layout.render_pass;

    VkFramebuffer vk_framebuffer;
    vkCreateFramebuffer(device, &create_info, nullptr, &vk_framebuffer);

    get_current_frame_context().destructors.push_back([=] {
        vkDestroyFramebuffer(device, vk_framebuffer, nullptr);
    });

    Framebuffer framebuffer{};
    framebuffer.framebuffer = vk_framebuffer;
    return framebuffer;
}

Texture Context::get_swapchain_texture() const {
    return swapchain_textures[swapchain_image_index];
}

Shader Context::acquire_shader(char* data, uint32_t size, Morpho::Vulkan::ShaderStage stage) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = size;
    info.pCode = reinterpret_cast<const uint32_t*>(data);

    VkShaderModule shader_module;
    VK_CHECK(vkCreateShaderModule(device, &info, nullptr, &shader_module), "Unable to create shader module.");

    Shader shader{};
    shader.shader_module = shader_module;
    shader.stage = stage;
    return shader;
}

VkExtent2D Context::get_swapchain_extent() const {
    return swapchain_extent;
}

Pipeline Context::create_pipeline(PipelineInfo &pipeline_info) {
    size_t hash = 0;
    VkPipelineShaderStageCreateInfo stages[(uint32_t)ShaderStage::MAX_VALUE];
    for (uint32_t i = 0; i < pipeline_info.shader_count; i++) {
        auto& shader = pipeline_info.shaders[i];
        stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[i].pNext = nullptr;
        stages[i].flags = 0;
        stages[i].stage = shader_stage_to_vulkan(shader.stage);
        stages[i].module = shader.shader_module;
        stages[i].pName = "main";
        stages[i].pSpecializationInfo = nullptr;
    }

    VkPipelineVertexInputStateCreateInfo vertex_input_state{};
    vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state.vertexBindingDescriptionCount = pipeline_info.binding_count;
    vertex_input_state.pVertexBindingDescriptions = pipeline_info.bindings;
    vertex_input_state.vertexAttributeDescriptionCount = pipeline_info.attribute_count;
    vertex_input_state.pVertexAttributeDescriptions = pipeline_info.attributes;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state{};
    input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state.pNext = nullptr;
    input_assembly_state.flags = 0;
    input_assembly_state.topology = pipeline_info.primitive_topology;
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
    rasterization_state.cullMode = pipeline_info.cull_mode;
    rasterization_state.frontFace = pipeline_info.front_face;
    rasterization_state.depthBiasEnable = pipeline_info.depth_bias_constant_factor != 0.0f || pipeline_info.depth_bias_slope_factor != 0.0f;
    rasterization_state.depthBiasConstantFactor = pipeline_info.depth_bias_constant_factor;
    rasterization_state.depthBiasSlopeFactor = pipeline_info.depth_bias_slope_factor;

    VkPipelineMultisampleStateCreateInfo multisample_state{};
    multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state.pNext = nullptr;
    multisample_state.flags = 0;
    multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_state.sampleShadingEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state{};
    depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_state.depthTestEnable = pipeline_info.depth_test_enabled;
    depth_stencil_state.depthWriteEnable = pipeline_info.depth_write_enabled;
    depth_stencil_state.depthCompareOp = pipeline_info.depth_compare_op;

    VkPipelineColorBlendAttachmentState color_blend_attachment_state = pipeline_info.blend_state;

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

    VkGraphicsPipelineCreateInfo vk_pipeline_info{};
    vk_pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    vk_pipeline_info.stageCount = pipeline_info.shader_count;
    vk_pipeline_info.pStages = stages;
    vk_pipeline_info.pVertexInputState = &vertex_input_state;
    vk_pipeline_info.pInputAssemblyState = &input_assembly_state;
    vk_pipeline_info.pTessellationState = nullptr;
    vk_pipeline_info.pViewportState = &viewport_state;
    vk_pipeline_info.pRasterizationState = &rasterization_state;
    vk_pipeline_info.pMultisampleState = &multisample_state;
    vk_pipeline_info.pDepthStencilState = &depth_stencil_state;
    vk_pipeline_info.pColorBlendState = &color_blend_state;
    vk_pipeline_info.pDynamicState = &dynamic_state;
    vk_pipeline_info.layout = pipeline_info.pipeline_layout->pipeline_layout;
    vk_pipeline_info.renderPass = pipeline_info.render_pass_layout->render_pass;
    vk_pipeline_info.subpass = 0;
    vk_pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    vk_pipeline_info.basePipelineIndex = 0;

    VkPipeline vk_pipeline{};
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &vk_pipeline_info, nullptr, &vk_pipeline);

    Pipeline pipeline{};
    pipeline.pipeline = vk_pipeline;
    pipeline.pipeline_layout = *pipeline_info.pipeline_layout;
    return pipeline;
}

Buffer Context::acquire_buffer(const BufferInfo& info) {
    VkBufferCreateInfo buffer_create_info{};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = info.size;
    buffer_create_info.usage = info.usage;

    VmaAllocationCreateInfo allocation_create_info{};
    allocation_create_info.usage = info.memory_usage;
    switch (info.map) {
        case BufferMap::PERSISTENTLY_MAPPED:
            allocation_create_info.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
        case BufferMap::CAN_BE_MAPPED:
            allocation_create_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            break;
        case BufferMap::NONE:
            break;
        default:
            assert(false);
            break;
    }

    VkBuffer vk_buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;

    vmaCreateBuffer(allocator, &buffer_create_info, &allocation_create_info, &vk_buffer, &allocation, &allocation_info);

    Buffer buffer{};
    buffer.buffer = vk_buffer;
    buffer.allocation = allocation;
    buffer.mapped = (char*)allocation_info.pMappedData;

    if (info.initial_data != nullptr) {
        assert(info.initial_data_size <= info.size && info.map != BufferMap::NONE);
        void* mapped;
        vmaMapMemory(allocator, allocation, &mapped);
        memcpy(mapped, info.initial_data, info.initial_data_size);
        vmaUnmapMemory(allocator, allocation);
    }
    return buffer;
}

void Context::map_memory(VmaAllocation allocation, void **map) {
    auto res = vmaMapMemory(allocator, allocation, map);
}

void Context::unmap_memory(VmaAllocation allocation) {
    vmaUnmapMemory(allocator, allocation);
}

Buffer Context::acquire_staging_buffer(const BufferInfo& info) {
    auto& frame_context = get_current_frame_context();
    auto buffer = acquire_buffer(info);
    release_buffer_on_frame_begin(buffer);
    return buffer;
}

void Context::release_buffer(Buffer buffer) {
    release_buffer_on_frame_begin(buffer);
}

void Context::update_buffer(const Buffer buffer, uint64_t offset, const void* data, uint64_t size)
{
    void* mapped;
    map_memory(buffer.allocation, &mapped);
    memcpy((char*)mapped + offset, data, size);
    unmap_memory(buffer.allocation);
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

DescriptorSet Context::acquire_descriptor_set(VkDescriptorSetLayout descriptor_set_layout) {
    DescriptorSet set;
    auto& frame_context = get_current_frame_context();
    while (frame_context.current_descriptor_pool_index < frame_context.descriptor_pools.size()) {
        auto& descriptor_pool = frame_context.descriptor_pools[frame_context.current_descriptor_pool_index];
        if (descriptor_pool.try_allocate_set(descriptor_set_layout, set)) {
            return set;
        }
        frame_context.current_descriptor_pool_index++;
    }
    VkDescriptorPoolSize pool_sizes[2];
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = 128;
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[1].descriptorCount = 128;
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 2;
    pool_info.pPoolSizes = pool_sizes;
    pool_info.maxSets = 16;
    VkDescriptorPool vulkan_descriptor_pool;
    vkCreateDescriptorPool(device, &pool_info, nullptr, &vulkan_descriptor_pool);
    frame_context.descriptor_pools.push_back(DescriptorPool(device, vulkan_descriptor_pool, 16));
    return acquire_descriptor_set(descriptor_set_layout);
}

PipelineLayout Context::create_pipeline_layout(const PipelineLayoutInfo& pipeline_layout_info)
{
    PipelineLayout pipeline_layout{};
    VkDescriptorSetLayout *descriptor_set_layouts = pipeline_layout.descriptor_set_layouts;
    VkPipelineLayoutCreateInfo vk_pipeline_layout_info{};
    vk_pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    vk_pipeline_layout_info.setLayoutCount = Limits::MAX_DESCRIPTOR_SET_COUNT;
    vk_pipeline_layout_info.pSetLayouts = pipeline_layout.descriptor_set_layouts;
    VkDescriptorPoolSize descriptor_pool_sizes[VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT + 1]{};
    for (uint32_t set_index = 0; set_index < Limits::MAX_DESCRIPTOR_SET_COUNT; set_index++) {
        if (pipeline_layout_info.set_binding_count[set_index] == 0) {
            descriptor_set_layouts[set_index] = empty_descriptor_set_layout;
            pipeline_layout.descriptor_pools[set_index] = VK_NULL_HANDLE;
            continue;
        }
        for (uint32_t j = 0; j <= VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT; j++)  {
            descriptor_pool_sizes[j].type = (VkDescriptorType)j;
            descriptor_pool_sizes[j].descriptorCount = 0;
        }
        for (uint32_t j = 0; j < pipeline_layout_info.set_binding_count[set_index]; j++) {
            auto& binding_info = pipeline_layout_info.set_binding_infos[set_index][j];
            descriptor_pool_sizes[binding_info.descriptorType].descriptorCount += binding_info.descriptorCount;
        }
        uint32_t non_zero_size_count = 0;
        for (uint32_t size_index = 0; size_index <= VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT; size_index++) {
            if (descriptor_pool_sizes[size_index].descriptorCount != 0) {
                descriptor_pool_sizes[non_zero_size_count++] = descriptor_pool_sizes[size_index];
            }
        }
        assert(non_zero_size_count != 0);
        for (uint32_t size_index = 0; size_index < non_zero_size_count; size_index++)  {
            descriptor_pool_sizes[size_index].descriptorCount *=
                pipeline_layout_info.max_descriptor_set_counts[set_index];
        }
        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.maxSets = pipeline_layout_info.max_descriptor_set_counts[set_index];
        pool_info.pPoolSizes = descriptor_pool_sizes;
        pool_info.poolSizeCount = non_zero_size_count;
        vkCreateDescriptorPool(device, &pool_info, nullptr, &pipeline_layout.descriptor_pools[set_index]);
        VkDescriptorSetLayoutCreateInfo vk_descriptor_set_layout_info{};
        vk_descriptor_set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        vk_descriptor_set_layout_info.bindingCount = pipeline_layout_info.set_binding_count[set_index];
        vk_descriptor_set_layout_info.pBindings = pipeline_layout_info.set_binding_infos[set_index];
        VK_CHECK(
            vkCreateDescriptorSetLayout(device, &vk_descriptor_set_layout_info, nullptr, &descriptor_set_layouts[set_index]),
            "Unable to create VkDescriptorSetLayout"
        );
    }
    VK_CHECK(
        vkCreatePipelineLayout(device, &vk_pipeline_layout_info, nullptr, &pipeline_layout.pipeline_layout),
        "Unable to create VkPipelineLayout"
    );
    return pipeline_layout;
}

void Context::destroy_pipline_layout(const PipelineLayout &pipeline_layout)
{
    vkDestroyPipelineLayout(device, pipeline_layout.pipeline_layout, nullptr);
    for (uint32_t i = 0; i < Limits::MAX_DESCRIPTOR_SET_COUNT; i++) {
        if (pipeline_layout.descriptor_set_layouts[i] != empty_descriptor_set_layout) {
            vkDestroyDescriptorSetLayout(device, pipeline_layout.descriptor_set_layouts[i], nullptr);
            vkResetDescriptorPool(device, pipeline_layout.descriptor_pools[i], 0);
            vkDestroyDescriptorPool(device, pipeline_layout.descriptor_pools[i], nullptr);
        }
    }
}

Texture Context::create_texture(
    VkExtent3D extent,
    VkFormat format,
    VkImageUsageFlags image_usage,
    VmaMemoryUsage memory_usage,
    uint32_t array_layers,
    VkImageCreateFlags flags
) {
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.flags = flags;
    image_info.extent = extent;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = format;
    image_info.mipLevels = 1;
    image_info.arrayLayers = array_layers;
    image_info.usage = image_usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocation_create_info{};
    allocation_create_info.usage = memory_usage;

    VkImage vk_image;
    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;
    vmaCreateImage(allocator, &image_info, &allocation_create_info, &vk_image, &allocation, &allocation_info);

    // Support 2D and Cube textures for now.
    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
    if (array_layers == 6 && (flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0) {
        view_type = VK_IMAGE_VIEW_TYPE_CUBE;
    }

    VkImageAspectFlags aspect = is_depth_format(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageViewCreateInfo image_view_info{};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.format = format;
    image_view_info.viewType = view_type;
    image_view_info.subresourceRange.aspectMask = aspect;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = array_layers;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = 1;
    image_view_info.image = vk_image;

    VkImageView vk_image_view;
    vkCreateImageView(device, &image_view_info, nullptr, &vk_image_view);

    Texture texture{};
    texture.image = vk_image;
    texture.image_view = vk_image_view;
    texture.allocation = allocation;
    texture.allocation_info = allocation_info;
    texture.format = format;
    texture.owns_image = true;
    return texture;
}

Texture Context::create_temporary_texture(
    VkExtent3D extent,
    VkFormat format,
    VkImageUsageFlags image_usage,
    VmaMemoryUsage memory_usage,
    uint32_t array_layers,
    VkImageCreateFlags flags
) {
    auto texture = create_texture(extent, format, image_usage, memory_usage, array_layers, flags);
    release_texture_on_frame_begin(texture);
    return texture;
}

Texture Context::create_texture_view(const Texture& texture, uint32_t base_array_layer, uint32_t layer_count)
{
    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;

    VkImageAspectFlags aspect = is_depth_format(texture.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageViewCreateInfo image_view_info{};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.format = texture.format;
    image_view_info.viewType = view_type;
    image_view_info.subresourceRange.aspectMask = aspect;
    image_view_info.subresourceRange.baseArrayLayer = base_array_layer;
    image_view_info.subresourceRange.layerCount = layer_count;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = 1;
    image_view_info.image = texture.image;

    VkImageView vk_image_view{};
    vkCreateImageView(device, &image_view_info, nullptr, &vk_image_view);

    Texture view{};
    view.format = texture.format;
    view.image = texture.image;
    view.image_view = vk_image_view;
    return view;
}

Texture Context::create_temporary_texture_view(const Texture& texture, uint32_t base_array_layer, uint32_t layer_count)
{
    auto view = create_texture_view(texture, base_array_layer, layer_count);
    release_texture_on_frame_begin(view);
    return view;
}

void Context::destroy_texture(Texture texture) {
    release_texture_on_frame_begin(texture);
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
    sampler_info.maxLod = 1.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.unnormalizedCoordinates = VK_FALSE;

    VkSampler vk_sampler;
    vkCreateSampler(device, &sampler_info, nullptr, &vk_sampler);

    Sampler sampler{};
    sampler.sampler = vk_sampler;
    return sampler;
}

DescriptorSet Context::create_descriptor_set(const PipelineLayout& pipeline_layout, uint32_t set_index) {
    if (pipeline_layout.descriptor_set_layouts[set_index] == empty_descriptor_set_layout) {
        DescriptorSet set{};
        set.descriptor_set = empty_descriptor_set;
        return set;
    }
    auto descriptor_set_layout = pipeline_layout.descriptor_set_layouts[set_index];
    VkDescriptorSetAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.pSetLayouts = &descriptor_set_layout;
    allocate_info.descriptorPool = pipeline_layout.descriptor_pools[set_index];
    allocate_info.descriptorSetCount = 1;
    VkDescriptorSet vk_descriptor_set = VK_NULL_HANDLE;
    VK_CHECK(
        vkAllocateDescriptorSets(device, &allocate_info, &vk_descriptor_set),
        "Unable to allocate VkDescriptorSet. Probably not enough decriptors in the pool."
    );
    assert(pipeline_layout.pipeline_layout != NULL);
    DescriptorSet descriptor_set{};
    descriptor_set.descriptor_set = vk_descriptor_set;
    descriptor_set.set_index = set_index;
    descriptor_set.pipeline_layout = pipeline_layout.pipeline_layout;
    return descriptor_set;
}

bool is_buffer_descriptor(VkDescriptorType type) {
    return  type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
        || type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
        || type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        || type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
        || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
        || type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
}

void Context::update_descriptor_set(
    const DescriptorSet& descriptor_set,
    DescriptorSetUpdateRequest* update_requests,
    const uint32_t request_count
) {
    const uint32_t write_batch_size = 128;
    VkWriteDescriptorSet writes[write_batch_size]{};
    union {
        VkDescriptorImageInfo image_info;
        VkDescriptorBufferInfo buffer_info;
    } descriptor_infos[write_batch_size]{};
    uint32_t current_batch_size = 0;
    uint32_t batch_count = (request_count + write_batch_size - 1) / write_batch_size;
    uint32_t remaining_request_count = request_count;
    uint32_t request_index = 0;
    for (uint32_t batch_index = 0; batch_index < batch_count; batch_index++) {
        const uint32_t write_count = std::min(write_batch_size, remaining_request_count);
        for (uint32_t write_index = 0; write_index < write_count; write_index++) {
            auto& write = writes[write_index];
            auto& request = update_requests[request_index++];
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = descriptor_set.descriptor_set;
            write.dstBinding = request.binding;
            write.descriptorType = request.descriptor_type;
            write.descriptorCount = 1;
            if (is_buffer_descriptor(write.descriptorType)) {
                auto buffer_info = request.descriptor_info.buffer_info;
                auto vk_buffer_info = &descriptor_infos[write_index].buffer_info;
                vk_buffer_info->buffer = buffer_info.buffer.buffer;
                vk_buffer_info->offset = buffer_info.offset;
                vk_buffer_info->range = buffer_info.range;
                write.pBufferInfo = vk_buffer_info;
                write.pImageInfo = nullptr;
            } else {
                auto texture_info = request.descriptor_info.texture_info;
                auto* vk_image_info = &descriptor_infos[write_index].image_info;
                vk_image_info->sampler = texture_info.sampler.sampler;
                vk_image_info->imageView = texture_info.texture.image_view;
                vk_image_info->imageLayout = is_depth_format(texture_info.texture.format)
                    ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                write.pImageInfo = vk_image_info;
                write.pBufferInfo = nullptr;
            }
        }
        vkUpdateDescriptorSets(device, write_count, writes, 0, nullptr);
        remaining_request_count -= write_batch_size;
    }
}

uint64_t Context::get_uniform_buffer_alignment() const {
    return min_uniform_buffer_offset_alignment;
}

VkFormat Context::get_swapchain_format() const {
    return swapchain_format;
}

VkRenderPass Context::create_render_pass(const RenderPassInfo& info) {
    assert(info.attachent_count == info.layout.info.attachent_count);
    VkRenderPassCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    VkAttachmentDescription attachment_descriptions[RenderPassInfo::max_attachment_count];
    VkAttachmentReference references[RenderPassInfo::max_attachment_count];
    uint32_t reference_index = 0;
    uint32_t preserve_indicies[RenderPassInfo::max_attachment_count];
    uint32_t preserve_index = 0;
    create_info.attachmentCount = info.attachent_count;
    create_info.pAttachments = attachment_descriptions;
    const auto& layout_info = info.layout.info;
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
    render_pass_info.layout.info = info;
    size_t hash = 0;
    VkRenderPass render_pass;
    hash_combine(hash, info);
    if (!render_pass_cache.try_get(hash, render_pass)) {
        render_pass = create_render_pass(render_pass_info);
    }
    RenderPassLayout render_pass_layout{};
    render_pass_layout.info = info;
    render_pass_layout.render_pass = render_pass;
    return render_pass_layout;
}

RenderPass Context::acquire_render_pass(const RenderPassInfo& info) {
    size_t hash = 0;
    VkRenderPass vk_render_pass;
    if (!render_pass_cache.try_get(hash, vk_render_pass)) {
        vk_render_pass = create_render_pass(info);
    }
    RenderPass render_pass{};
    render_pass.layout = info.layout;
    render_pass.render_pass = vk_render_pass;
    return render_pass;
}

}
