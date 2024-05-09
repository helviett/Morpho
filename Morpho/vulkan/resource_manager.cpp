#include "resource_manager.hpp"
#include "context.hpp"
#include <stb_ds.h>
#include "common/utils.hpp"

namespace Morpho::Vulkan {

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

Handle<Texture> ResourceManager::register_texture(Texture texture) {
    return textures.add(texture);
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

void ResourceManager::next_frame() {
    if (!committed) {
        return;
    }
    memory_barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    src_stages = dst_stages = 0;
    committed = 0;
    cmd_pool->next_frame();
    *pre_cmd = cmd_pool->allocate();
    *post_cmd = cmd_pool->allocate();
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
    rm->pre_cmd = (CommandBuffer*)malloc(sizeof(CommandBuffer) * 2);
    rm->post_cmd = rm->pre_cmd + 1;
    rm->allocator = context->allocator;
    rm->next_frame();
    rm->queue = context->graphics_queue;
    rm->device = context->device;
    return g_resource_manager = rm;
}

void ResourceManager::destroy(ResourceManager* rm) {
    free(rm->pre_cmd);
    rm->context->destroy_cmd_pool(rm->cmd_pool);
    free(rm);
    g_resource_manager = nullptr;
}

ResourceManager* ResourceManager::get() {
    return g_resource_manager;
}

}
