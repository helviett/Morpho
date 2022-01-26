#pragma once
#include <vulkan/vulkan.h>
#include "vma.hpp"

namespace Morpho::Vulkan {

class Context;

class Buffer {
public:
    Buffer();
    Buffer(Context* context, VkBuffer buffer, VmaAllocation allocation, VmaAllocationInfo allocation_info);

    void update(const void* data, uint32_t size);
    VkBuffer get_buffer() const;
private:
    Context* context;
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;
};

}