#pragma once
#include "vulkan/resources.hpp"
#include "vulkan/resource_manager.hpp"

struct FixedSizeAllocatorInfo {
    Morpho::Vulkan::ResourceManager* resource_manager;
    Morpho::Handle<Morpho::Vulkan::Buffer> buffer;
    uint64_t buffer_offset;
    uint64_t item_size;
    uint64_t offset_alignment;
    uint64_t max_item_count; // 0 means unbounded
};

class FixedSizeAllocator {
public:
    static FixedSizeAllocator create(const FixedSizeAllocatorInfo& info);

    static uint64_t compute_buffer_size(uint64_t data_size, uint64_t item_count, uint64_t offset_alignment);

    uint64_t get_offset(uint64_t index);
    uint8_t* get_mapped_ptr(uint64_t index);
private:
    Morpho::Handle<Morpho::Vulkan::Buffer> buffer;
    uint64_t base_offset;
    uint64_t available_size;
    uint64_t aligned_data_size;
    uint8_t* mapped;
};

struct UniformBufferBumpAllocatorInfo {
    Morpho::Vulkan::ResourceManager* resource_manager;
    uint64_t backing_buffer_size = 1024 * 1024 * 16;
    uint64_t alignment;
    uint64_t frames_in_flight_count;
};

struct UniformAllocation
{
    Morpho::Handle<Morpho::Vulkan::Buffer> buffer;
    uint64_t offset;
    uint8_t* ptr;
};

class UniformBufferBumpAllocator {
public:
    static void init(
        const UniformBufferBumpAllocatorInfo& info,
        UniformBufferBumpAllocator* allocator
    );

    UniformAllocation allocate(uint64_t size);
    void next_frame();
private:
    struct UsedBuffer {
        uint64_t frame;
        uint64_t used_offset;
        Morpho::Handle<Morpho::Vulkan::Buffer> buffer;
        uint8_t* base_ptr;
    };

    struct FreeBuffer {
        Morpho::Handle<Morpho::Vulkan::Buffer> buffer;
        uint8_t* base_ptr;
    };

    Morpho::Vulkan::ResourceManager* resource_manager;
    uint64_t backing_buffer_size;
    uint64_t alignment;
    uint64_t frame;
    uint64_t frames_in_flight_count;
    FreeBuffer* free_buffers;
    UsedBuffer* used_buffers;
};
