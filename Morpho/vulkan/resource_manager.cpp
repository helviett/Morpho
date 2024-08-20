#include "resource_manager.hpp"
#include "context.hpp"
#include <stb_ds.h>
#include "common/utils.hpp"

namespace Morpho::Vulkan {

static inline bool is_depth_format(VkFormat format) {
    return format == VK_FORMAT_D16_UNORM
        || format == VK_FORMAT_D16_UNORM_S8_UINT
        || format == VK_FORMAT_D24_UNORM_S8_UINT
        || format == VK_FORMAT_D32_SFLOAT
        || format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

static bool is_buffer_descriptor(VkDescriptorType type) {
    return  type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
        || type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
        || type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        || type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
        || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
        || type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
}

static void derive_stages_and_access_from_buffer_usage(
    VkBufferUsageFlags usage,
    VkPipelineStageFlags* stages,
    VkAccessFlags* access
) {
    if (usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) {
        *stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        *access |= VK_ACCESS_TRANSFER_READ_BIT;
    }
    if (usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) {
        *stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        *access |= VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    if (usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {
        *stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
            | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
            | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        *access |= VK_ACCESS_UNIFORM_READ_BIT;
    }
    if (usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
        *stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
            | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
            | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        *access |= VK_ACCESS_SHADER_READ_BIT
            | VK_ACCESS_SHADER_WRITE_BIT;
    }
    if (usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) {
        *stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        *access |= VK_ACCESS_INDEX_READ_BIT;
    }
    if (usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) {
        *stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        *access |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }
    if (usage & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) {
        *stages |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        *access |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    }
}

static void derive_stages_access_final_layout_from_texture_usage(
        VkImageUsageFlags usage,
        VkPipelineStageFlags* stages,
        VkAccessFlags* access,
        VkImageLayout* layout,
        bool* ambigious
) {
    *ambigious = false;
    if (usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
        *stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        *access |= VK_ACCESS_TRANSFER_READ_BIT;
    }
    if (usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        *stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        *access |= VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    if (usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
        *stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
            | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        *access |= VK_ACCESS_SHADER_READ_BIT;
        *ambigious = *layout != VK_IMAGE_LAYOUT_UNDEFINED;
        *layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    if (usage & VK_IMAGE_USAGE_STORAGE_BIT) {
        *stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
            | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
            | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        *access |= VK_ACCESS_SHADER_READ_BIT
            | VK_ACCESS_SHADER_WRITE_BIT;
        *ambigious = *layout != VK_IMAGE_LAYOUT_UNDEFINED;
        *layout = VK_IMAGE_LAYOUT_GENERAL;

    }
    if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
        *stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        *access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
            | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        *ambigious = *layout != VK_IMAGE_LAYOUT_UNDEFINED;
        *layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        *stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        *access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        *ambigious = *layout != VK_IMAGE_LAYOUT_UNDEFINED;
        *layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    // if (usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) {}
    // if (usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) {}
}

Handle<Buffer> ResourceManager::create_buffer(const BufferInfo& info, uint8_t** mapped_ptr) {
    assert(
        (mapped_ptr == nullptr && info.map != BufferMap::PERSISTENTLY_MAPPED)
        || (mapped_ptr != nullptr && info.map != BufferMap::NONE)
    );
    Buffer buffer = create_vk_buffer(info);
    Handle<Buffer> handle = buffers.add(buffer);
    bool create_mapped = mapped_ptr != nullptr && info.map != BufferMap::NONE;
    if (create_mapped) {
        map_buffer_helper(&buffer);
        *mapped_ptr = buffer.mapped;
    }

    if (info.initial_data == nullptr) {
        return handle;
    }
    assert(info.initial_data_size <= info.size);
    if (info.map == BufferMap::NONE) {
        StagingBuffer* sb = acquire_staging_buffer(info.initial_data_size);
        memcpy(sb->write_ptr, info.initial_data, info.initial_data_size);
        post_cmd->copy_buffer(
            sb->buffer,
            buffer,
            {
                .srcOffset = sb->write_offset,
                .dstOffset = 0,
                .size = info.initial_data_size,
            }
        );
        memory_barrier.srcAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        derive_stages_and_access_from_buffer_usage(info.usage, &dst_stages, &memory_barrier.dstAccessMask);
        need_submit = 1;
    } else {
        if (create_mapped) {
            memcpy(buffer.mapped, info.initial_data, info.initial_data_size);
        } else {
            map_buffer_helper(&buffer);
            memcpy(buffer.mapped, info.initial_data, info.initial_data_size);
            unmap_buffer_helper(&buffer);
        }
    }
    return handle;
}

Handle<Texture> ResourceManager::create_texture(const TextureInfo& texture_info) {
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.flags = texture_info.flags;
    image_info.extent = texture_info.extent;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = texture_info.format;
    image_info.mipLevels = texture_info.mip_level_count;
    image_info.arrayLayers = texture_info.array_layer_count;
    image_info.usage = texture_info.image_usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkPipelineStageFlags texture_dst_stages{};
    VkAccessFlags texture_dst_access{};
    VkImageLayout final_layout{};
    bool is_ambigious = false;
    derive_stages_access_final_layout_from_texture_usage(
        texture_info.image_usage,
        &texture_dst_stages,
        &texture_dst_access,
        &final_layout,
        &is_ambigious
    );
    final_layout = texture_info.initial_layout == VK_IMAGE_LAYOUT_UNDEFINED
        ? final_layout : texture_info.initial_layout;

    if (is_ambigious && texture_info.initial_layout == VK_IMAGE_LAYOUT_UNDEFINED) {
        fprintf(
            stdout,
            "[Warning] Ambigious initial layout; Selecting %d; Consider providing your own initial layout",
            final_layout
        );
    }

    VmaAllocationCreateInfo allocation_create_info{};
    allocation_create_info.usage = texture_info.memory_usage;

    VkImage vk_image;
    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;
    vmaCreateImage(allocator, &image_info, &allocation_create_info, &vk_image, &allocation, &allocation_info);

    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
    if (texture_info.array_layer_count == 6 && (texture_info.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0) {
        view_type = VK_IMAGE_VIEW_TYPE_CUBE;
    } else if (texture_info.array_layer_count > 1) {
        view_type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    }

    VkImageAspectFlags aspect = derive_aspect(texture_info.format);

    VkImageViewCreateInfo image_view_info{};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.format = texture_info.format;
    image_view_info.viewType = view_type;
    image_view_info.subresourceRange.aspectMask = aspect;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = texture_info.array_layer_count;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = texture_info.mip_level_count;
    image_view_info.image = vk_image;

    VkImageView vk_image_view;
    vkCreateImageView(device, &image_view_info, nullptr, &vk_image_view);

    Texture texture{};
    texture.image = vk_image;
    texture.image_view = vk_image_view;
    texture.allocation = allocation;
    texture.allocation_info = allocation_info;
    texture.format = texture_info.format;
    texture.aspect = aspect;
    texture.owns_image = true;
    Handle<Texture> handle = textures.add(texture);

    VkImageMemoryBarrier post_barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    post_barrier.subresourceRange.aspectMask = aspect;
    post_barrier.subresourceRange.baseMipLevel = 0;
    post_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    post_barrier.subresourceRange.baseArrayLayer = 0;
    post_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    post_barrier.image = texture.image;

    if (texture_info.initial_data == nullptr) {
        src_stages |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stages |= texture_dst_stages;
        post_barrier.srcAccessMask = 0;
        post_barrier.dstAccessMask = texture_dst_access;
        post_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        post_barrier.newLayout = final_layout;
        post_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        post_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        arrput(post_barriers, post_barrier);

        need_submit = 1;

        return handle;
    }

    VkImageMemoryBarrier pre_barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    pre_barrier.subresourceRange.aspectMask = aspect;
    pre_barrier.subresourceRange.baseMipLevel = 0;
    pre_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    pre_barrier.subresourceRange.baseArrayLayer = 0;
    pre_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    pre_barrier.image = texture.image;
    pre_barrier.srcAccessMask = 0;
    pre_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    pre_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    pre_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    arrput(pre_barriers, pre_barrier);

    need_submit = 1;
    StagingBuffer* sb = acquire_staging_buffer(texture_info.initial_data_size);
    memcpy(sb->write_ptr, texture_info.initial_data, texture_info.initial_data_size);
    post_cmd->copy_buffer_to_image(
        sb->buffer,
        texture,
        BufferTextureCopyRegion{
            .buffer_offset = sb->write_offset,
            .texture_extent = texture_info.extent,
            .texture_subresource = {
                .layer_count = texture_info.array_layer_count,
            },
        }
    );
    src_stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst_stages |= texture_dst_stages;
    post_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    post_barrier.dstAccessMask = texture_dst_access;
    post_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    post_barrier.newLayout = final_layout;

    arrput(post_barriers, post_barrier);

    return handle;
}

Handle<Texture> ResourceManager::create_texture_view(
    Handle<Texture> texture_handle,
    uint32_t base_array_layer,
    uint32_t layer_count
) {
    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;

    Texture texture = textures.get(texture_handle);

    VkImageAspectFlags aspect = texture.aspect;

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
    view.aspect = texture.aspect;
    view.image = texture.image;
    view.image_view = vk_image_view;

    return textures.add(view);
}

Handle<Shader> ResourceManager::create_shader(char* data, uint32_t size, Morpho::Vulkan::ShaderStage stage) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = size;
    info.pCode = reinterpret_cast<const uint32_t*>(data);

    VkShaderModule shader_module;
    VK_CHECK(vkCreateShaderModule(device, &info, nullptr, &shader_module), "Unable to create shader module.");

    Shader shader{};
    shader.shader_module = shader_module;
    shader.stage = stage;
    return shaders.add(shader);
}

Handle<RenderPassLayout> ResourceManager::create_render_pass_layout(const RenderPassLayoutInfo& info) {
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
    VkRenderPass render_pass;
    render_pass = create_vk_render_pass(render_pass_info, info);
    RenderPassLayout render_pass_layout{};
    render_pass_layout.info = info;
    render_pass_layout.render_pass = render_pass;
    return render_pass_layouts.add(render_pass_layout);
}

Handle<RenderPass> ResourceManager::create_render_pass(const RenderPassInfo& info) {
    VkRenderPass vk_render_pass;
    vk_render_pass = create_vk_render_pass(info, get_render_pass_layout(info.layout).info);
    RenderPass render_pass{};
    render_pass.layout = info.layout;
    render_pass.render_pass = vk_render_pass;
    return render_passes.add(render_pass);
}

Handle<PipelineLayout> ResourceManager::create_pipeline_layout(const PipelineLayoutInfo& pipeline_layout_info)
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
    return pipeline_layouts.add(pipeline_layout);
}

Handle<DescriptorSet> ResourceManager::create_descriptor_set(Handle<PipelineLayout> pipeline_layout_handle, uint32_t set_index) {
    PipelineLayout pipeline_layout = pipeline_layouts.get(pipeline_layout_handle);
    if (pipeline_layout.descriptor_set_layouts[set_index] == empty_descriptor_set_layout) {
        DescriptorSet set{};
        set.descriptor_set = empty_descriptor_set;
        // TODO: return empty_ds_handle;
        return descriptor_sets.add(set);
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
    return descriptor_sets.add(descriptor_set);
}

Handle<Sampler> ResourceManager::create_sampler(
    const SamplerInfo& info
) {
    bool use_all = info.address_mode_all != VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
    VkSamplerCreateInfo vk_sampler_info{};
    vk_sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    vk_sampler_info.addressModeU = use_all ? info.address_mode_all : info.address_mode_u;
    vk_sampler_info.addressModeV = use_all ? info.address_mode_all : info.address_mode_u;
    vk_sampler_info.addressModeW = use_all ? info.address_mode_all : info.address_mode_u;
    vk_sampler_info.anisotropyEnable = info.max_anisotropy != 0.0f;
    vk_sampler_info.maxAnisotropy = info.max_anisotropy;
    vk_sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    vk_sampler_info.minFilter = info.min_filter;
    vk_sampler_info.magFilter = info.mag_filter;
    vk_sampler_info.compareEnable = info.compare_enable;
    vk_sampler_info.compareOp = info.compare_op;
    vk_sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    vk_sampler_info.maxLod = info.max_lod;
    vk_sampler_info.minLod = info.min_lod;
    vk_sampler_info.unnormalizedCoordinates = VK_FALSE;

    VkSampler vk_sampler;
    vkCreateSampler(device, &vk_sampler_info, nullptr, &vk_sampler);

    Sampler sampler{};
    sampler.sampler = vk_sampler;
    return samplers.add(sampler);
}

Handle<Pipeline> ResourceManager::create_pipeline(const PipelineInfo &pipeline_info) {
    VkPipelineShaderStageCreateInfo stages[(uint32_t)ShaderStage::MAX_VALUE];
    ResourceManager* rm = ResourceManager::get();
    for (uint32_t i = 0; i < pipeline_info.shader_count; i++) {
        Shader shader = rm->get_shader(pipeline_info.shaders[i]);
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
    rasterization_state.depthClampEnable = pipeline_info.depth_clamp_enabled;

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
    vk_pipeline_info.layout = rm->get_pipeline_layout(pipeline_info.pipeline_layout).pipeline_layout;
    vk_pipeline_info.renderPass = rm->get_render_pass_layout(pipeline_info.render_pass_layout).render_pass;
    vk_pipeline_info.subpass = 0;
    vk_pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    vk_pipeline_info.basePipelineIndex = 0;

    VkPipeline vk_pipeline{};
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &vk_pipeline_info, nullptr, &vk_pipeline);

    Pipeline pipeline{};
    pipeline.pipeline = vk_pipeline;
    pipeline.pipeline_layout = pipeline_info.pipeline_layout;
    return pipelines.add(pipeline);
}

Handle<Texture> ResourceManager::register_texture(Texture texture) {
    return textures.add(texture);
}

void ResourceManager::update_descriptor_set(
    Handle<DescriptorSet> descriptor_set,
    Span<const DescriptorSetUpdateRequest> update_requests
) {
    const uint32_t write_batch_size = 128;
    VkWriteDescriptorSet writes[write_batch_size]{};
    const uint32_t info_size = std::max(sizeof(VkDescriptorImageInfo), sizeof(VkDescriptorBufferInfo)) * 1024;
    uint8_t vk_infos[info_size];
    uint32_t current_write_index = 0;
    uint32_t infos_offset = 0;
    ResourceManager* rm = ResourceManager::get();
    VkDescriptorSet vk_ds = rm->get_descriptor_set(descriptor_set).descriptor_set;
    for (uint32_t request_index = 0; request_index < update_requests.size(); request_index++) {
        const DescriptorSetUpdateRequest& request = update_requests[request_index];
        VkWriteDescriptorSet* write = &writes[current_write_index];
        *write = {};
        write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write->dstSet = vk_ds;
        write->dstBinding = request.binding;
        write->descriptorType = request.descriptor_type;
        write->dstArrayElement = 0;
        if (is_buffer_descriptor(write->descriptorType)) {
            uint32_t descriptor_count = request.texture_infos.size();
            uint32_t current_texture_info = 0;
            write->pImageInfo = nullptr;
            while (descriptor_count > 0) {
                uint32_t slot_count = std::min(
                    descriptor_count,
                    (info_size - infos_offset) / (uint32_t)sizeof(VkDescriptorBufferInfo)
                );
                if (slot_count == 0) {
                    // Ran out of space so have to flush.
                    vkUpdateDescriptorSets(device, current_write_index + 1, writes, 0, nullptr);
                    write = &writes[0];
                    write->dstArrayElement += write->descriptorCount;
                    current_write_index = infos_offset = 0;
                    continue;
                }
                VkDescriptorBufferInfo* vk_buffer_infos = (VkDescriptorBufferInfo*)(vk_infos + infos_offset);
                write->pBufferInfo = vk_buffer_infos;
                for (uint32_t i = 0; i < slot_count; i++) {
                    VkDescriptorBufferInfo& vk_buffer_info = vk_buffer_infos[i];
                    const BufferDescriptorInfo& buffer_info = request.buffer_infos[current_texture_info + i];
                    vk_buffer_info.buffer = rm->get_buffer(buffer_info.buffer).buffer;
                    vk_buffer_info.offset = buffer_info.offset;
                    vk_buffer_info.range = buffer_info.range;
                }
                descriptor_count -= slot_count;
                write->descriptorCount = slot_count;
            }
        } else {
            uint32_t descriptor_count = request.texture_infos.size();
            uint32_t current_texture_info = 0;
            write->pBufferInfo = nullptr;
            while (descriptor_count > 0) {
                uint32_t slot_count = std::min(
                    descriptor_count,
                    (info_size - infos_offset) / (uint32_t)sizeof(VkDescriptorImageInfo)
                );
                if (slot_count == 0) {
                    // Ran out of space so have to flush.
                    vkUpdateDescriptorSets(device, current_write_index + 1, writes, 0, nullptr);
                    write = &writes[0];
                    write->dstArrayElement += write->descriptorCount;
                    current_write_index = infos_offset = 0;
                    continue;
                }
                VkDescriptorImageInfo* vk_image_infos = (VkDescriptorImageInfo*)(vk_infos + infos_offset);
                write->pImageInfo = vk_image_infos;
                for (uint32_t i = 0; i < slot_count; i++) {
                    VkDescriptorImageInfo& vk_image_info = vk_image_infos[i];
                    const TextureDescriptorInfo& texture_info = request.texture_infos[current_texture_info + i];
                    Texture texture = rm->get_texture(texture_info.texture);
                    vk_image_info = {};
                    vk_image_info.sampler = get_sampler(texture_info.sampler).sampler;
                    vk_image_info.imageView = texture.image_view;
                    vk_image_info.imageLayout = is_depth_format(texture.format)
                        ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                        : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                }
                descriptor_count -= slot_count;
                write->descriptorCount = slot_count;
            }
        }
        current_write_index++;
        // Ensures that none of the branch run into the situation when it can't write a single descriptor.
        // It simplifies code.
        if ((info_size - info_size) / std::max(sizeof(VkDescriptorImageInfo), sizeof(VkDescriptorBufferInfo)) == 0) {
            vkUpdateDescriptorSets(device, current_write_index, writes, 0, nullptr);
            current_write_index = infos_offset = 0;
        }
    }
    if (current_write_index != 0) {
        vkUpdateDescriptorSets(device, current_write_index, writes, 0, nullptr);
    }
}

void ResourceManager::commit() {
    if (!need_submit) {
        return;
    }

    VkCommandBuffer pre_vk_cmd = pre_cmd->get_vulkan_handle();
    if (arrlen(pre_barriers) != 0) {
        VkMemoryBarrier empty{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        vkCmdPipelineBarrier(
            pre_vk_cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            1,
            &empty,
            0,
            nullptr,
            arrlen(pre_barriers),
            pre_barriers
        );
    }
    vkEndCommandBuffer(pre_vk_cmd);

    VkCommandBuffer post_vk_cmd = post_cmd->get_vulkan_handle();

    vkCmdPipelineBarrier(
        post_vk_cmd,
        src_stages,
        dst_stages,
        0,
        1,
        &memory_barrier,
        0,
        nullptr,
        arrlen(post_barriers),
        post_barriers
    );
    vkEndCommandBuffer(post_vk_cmd);
    VkCommandBuffer vk_cmds[2] = { pre_vk_cmd, post_vk_cmd, };
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pCommandBuffers = vk_cmds;
    submit_info.commandBufferCount = 2;
    vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
    arrsetlen(pre_barriers, 0);
    arrsetlen(post_barriers, 0);
    committed = 1;
    need_submit = 0;
}

Buffer ResourceManager::get_buffer(Handle<Buffer> handle) {
    assert(buffers.is_valid(handle));
    return buffers.get(handle);
}

Texture ResourceManager::get_texture(Handle<Texture> handle) {
    assert(textures.is_valid(handle));
    return textures.get(handle);
}

Shader ResourceManager::get_shader(Handle<Shader> handle) {
    assert(shaders.is_valid(handle));
    return shaders.get(handle);
}

RenderPassLayout ResourceManager::get_render_pass_layout(Handle<RenderPassLayout> handle) {
    assert(render_pass_layouts.is_valid(handle));
    return render_pass_layouts.get(handle);
}

RenderPass ResourceManager::get_render_pass(Handle<RenderPass> handle) {
    assert(render_passes.is_valid(handle));
    return render_passes.get(handle);
}

PipelineLayout ResourceManager::get_pipeline_layout(Handle<PipelineLayout> handle) {
    assert(pipeline_layouts.is_valid(handle));
    return pipeline_layouts.get(handle);
}

DescriptorSet ResourceManager::get_descriptor_set(Handle<DescriptorSet> handle) {
    assert(descriptor_sets.is_valid(handle));
    return descriptor_sets.get(handle);
}

Sampler ResourceManager::get_sampler(Handle<Sampler> handle) {
    assert(samplers.is_valid(handle));
    return samplers.get(handle);
}

Pipeline ResourceManager::get_pipeline(Handle<Pipeline> handle) {
    assert(pipelines.is_valid(handle));
    return pipelines.get(handle);
}

void ResourceManager::next_frame() {
    if (!committed) {
        return;
    }
    memory_barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    src_stages = dst_stages = 0;
    committed = 0;
    cmd_pool->next_frame();
    pre_cmd = cmd_pool->allocate();
    post_cmd = cmd_pool->allocate();
    frame = (frame + 1) % 2;
    for (int32_t i = arrlen(used_staging_buffers) - 1; i >= 0; i--) {
        StagingBuffer* sb = &used_staging_buffers[i];
        if (sb->frame_acquired == frame) {
            sb->used_offset = sb->write_offset = 0;
            sb->write_ptr = sb->buffer.mapped;
            arrput(free_staging_buffers, *sb);
            arrdelswap(used_staging_buffers, i);
        }
    }
}

ResourceManager::StagingBuffer* ResourceManager::acquire_staging_buffer(VkDeviceSize size) {
    for (uint32_t i = 0; i < arrlen(used_staging_buffers); i++) {
        StagingBuffer* sb = &used_staging_buffers[i];
        if (sb->size - sb->used_offset >= size) {
            sb->write_ptr = sb->buffer.mapped + sb->used_offset;
            sb->write_offset = sb->used_offset;
            sb->used_offset += size;
            sb->frame_acquired = frame;
            return sb;
        }
    }
    for (uint32_t i = 0; i < arrlen(free_staging_buffers); i++) {
        StagingBuffer* sb = &free_staging_buffers[i];
        if (sb->size >= size) {
            sb->write_ptr = sb->buffer.mapped + sb->used_offset;
            sb->write_offset = sb->used_offset;
            sb->used_offset += size;
            sb->frame_acquired = frame;
            arrput(used_staging_buffers, *sb);
            arrdelswap(free_staging_buffers, i);
            return &arrlast(used_staging_buffers);
        }
    }
    size = max(size, default_staging_buffer_size);
    Buffer buffer = create_vk_buffer({
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .map = BufferMap::PERSISTENTLY_MAPPED,
    });
    StagingBuffer staging_buffer = {
        .buffer = buffer,
        .size = size,
        .write_ptr = (uint8_t*)buffer.mapped,
        .write_offset = 0,
        .used_offset = size,
        .frame_acquired = frame,
    };
    arrput(used_staging_buffers, staging_buffer);
    return &used_staging_buffers[arrlen(used_staging_buffers) - 1];
}

Buffer ResourceManager::create_vk_buffer(const BufferInfo& info) {
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
    buffer.mapped = (uint8_t*)allocation_info.pMappedData;

    return buffer;
}

VkRenderPass ResourceManager::create_vk_render_pass(const RenderPassInfo& info, const RenderPassLayoutInfo& layout_info) {
    assert(info.attachent_count == layout_info.attachent_count);
    VkRenderPassCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    VkAttachmentDescription attachment_descriptions[RenderPassInfo::max_attachment_count];
    VkAttachmentReference references[RenderPassInfo::max_attachment_count];
    uint32_t reference_index = 0;
    uint32_t preserve_indicies[RenderPassInfo::max_attachment_count];
    uint32_t preserve_index = 0;
    create_info.attachmentCount = info.attachent_count;
    create_info.pAttachments = attachment_descriptions;
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
        //attachment_descriptions[subpass_info.depth_attachment.value()].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
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


void ResourceManager::map_buffer_helper(Buffer* buffer) {
    vmaMapMemory(allocator, buffer->allocation, (void**)&buffer->mapped);
}

void ResourceManager::unmap_buffer_helper(Buffer* buffer) {
    vmaUnmapMemory(allocator, buffer->allocation);
    buffer->mapped = nullptr;
}

ResourceManager* g_resource_manager = nullptr;

ResourceManager* ResourceManager::create(Context* context) {
    ResourceManager* rm = (ResourceManager*)malloc(sizeof(ResourceManager));
    memset(rm, 0, sizeof(ResourceManager));
    context->create_cmd_pool(&rm->cmd_pool);
    rm->committed = 1;
    rm->allocator = context->allocator;
    rm->next_frame();
    rm->queue = context->graphics_queue;
    rm->device = context->device;
    VkDescriptorSetLayoutCreateInfo vk_descriptor_set_layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, };
    vkCreateDescriptorSetLayout(rm->device, &vk_descriptor_set_layout_info, nullptr, &rm->empty_descriptor_set_layout);
    VkDescriptorPoolCreateInfo vk_descriptor_pool_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, };
    vk_descriptor_pool_info.maxSets = 1;
    vkCreateDescriptorPool(rm->device, &vk_descriptor_pool_info, nullptr, &rm->empty_descriptor_pool);
    VkDescriptorSetAllocateInfo vk_descriptor_set_allocate_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, };
    vk_descriptor_set_allocate_info.pSetLayouts = &rm->empty_descriptor_set_layout;
    vk_descriptor_set_allocate_info.descriptorSetCount = 1;
    vk_descriptor_set_allocate_info.descriptorPool = rm->empty_descriptor_pool;
    vkAllocateDescriptorSets(rm->device, &vk_descriptor_set_allocate_info, &rm->empty_descriptor_set);
    return g_resource_manager = rm;
}

void ResourceManager::destroy(ResourceManager* rm) {
    rm->context->destroy_cmd_pool(rm->cmd_pool);
    free(rm);
    g_resource_manager = nullptr;
}

ResourceManager* ResourceManager::get() {
    return g_resource_manager;
}

}
