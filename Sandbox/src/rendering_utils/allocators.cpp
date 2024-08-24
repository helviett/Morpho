#include "allocators.hpp"
#include "common/utils.hpp"
#include "stb_ds.h"

using namespace Morpho;
using namespace Morpho::Vulkan;

FixedSizeAllocator FixedSizeAllocator::create(const FixedSizeAllocatorInfo& info) {
    assert(is_pow2(info.offset_alignment));
    assert(info.resource_manager != nullptr);
    FixedSizeAllocator allocator{};
    allocator.buffer = info.buffer;
    allocator.base_offset = Morpho::align_up_pow2(info.buffer_offset, info.offset_alignment);
    allocator.aligned_data_size = Morpho::align_up_pow2(info.item_size, info.offset_alignment);
    allocator.mapped = info.resource_manager->map_buffer(info.buffer);
    uint64_t buffer_slice_size = info.resource_manager->get_buffer_size(info.buffer) - allocator.base_offset;
    allocator.available_size = info.max_item_count > 0
        ? allocator.aligned_data_size * info.max_item_count
        : align_down(buffer_slice_size, allocator.aligned_data_size);
    assert(allocator.available_size <= buffer_slice_size);
    return allocator;
}

uint64_t FixedSizeAllocator::compute_buffer_size(
    uint64_t data_size,
    uint64_t item_count,
    uint64_t offset_alignment
) {
    return Morpho::align_up_pow2(data_size, offset_alignment) * item_count;
}

uint64_t FixedSizeAllocator::get_offset(uint64_t index) {
    assert(aligned_data_size * index < available_size);
    return base_offset + aligned_data_size * index;
}

uint8_t* FixedSizeAllocator::get_mapped_ptr(uint64_t index) {
    return mapped + get_offset(index);
}

void UniformBufferBumpAllocator::init(
        const UniformBufferBumpAllocatorInfo& info,
        UniformBufferBumpAllocator* allocator
) {
    assert(allocator != nullptr);
    assert(info.frames_in_flight_count != 0);
    assert(is_pow2(info.alignment));
    memset(allocator, 0, sizeof(UniformBufferBumpAllocator));
    allocator->resource_manager = info.resource_manager;
    allocator->backing_buffer_size = info.backing_buffer_size;
    allocator->alignment = info.alignment;
    allocator->frames_in_flight_count = info.frames_in_flight_count;
}

UniformAllocation UniformBufferBumpAllocator::allocate(uint64_t size) {
    assert(size <= backing_buffer_size);
    if (arrlen(used_buffers) > 0) {
        UsedBuffer* buffer = &arrlast(used_buffers);
        if (buffer->frame == frame && buffer->used_offset + size <= backing_buffer_size) {
            UniformAllocation result = {
                .buffer = buffer->buffer,
                .offset = buffer->used_offset,
                .ptr = buffer->base_ptr + buffer->used_offset,
            };
            buffer->used_offset += size;
            buffer->used_offset = align_up_pow2(buffer->used_offset, alignment);
            return result;
        }
    }
    if (arrlen(free_buffers) == 0) {
        Handle<Buffer> handle = resource_manager->create_buffer({
            .size = backing_buffer_size,
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .map = BufferMap::PERSISTENTLY_MAPPED,
        });
        FreeBuffer fb { .buffer = handle, .base_ptr = resource_manager->map_buffer(handle), };
        arrput(free_buffers, fb);
    }
    FreeBuffer fb = arrpop(free_buffers);
    UsedBuffer used{
        .frame = frame,
        .used_offset = align_up_pow2(size, alignment),
        .buffer = fb.buffer,
        .base_ptr = fb.base_ptr,
    };
    arrput(used_buffers, used);
    return {
        .buffer = used.buffer,
        .offset = 0,
        .ptr = used.base_ptr
    };
}

void UniformBufferBumpAllocator::next_frame() {
    frame++;
    uint64_t freed_count = 0;
    for (uint64_t i = 0; i < arrlen(used_buffers); i++) {
        if (used_buffers[i].frame + frames_in_flight_count <= frame) {
            FreeBuffer fb { .buffer = used_buffers[i].buffer, .base_ptr = used_buffers[i].base_ptr, };
            arrput(free_buffers, fb);
            freed_count++;
        }
    }
    if (freed_count > 0) {
        arrdeln(used_buffers, 0, freed_count);
    }
}
