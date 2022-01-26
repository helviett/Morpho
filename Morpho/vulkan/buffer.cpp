#include "buffer.hpp"
#include "context.hpp"

namespace Morpho::Vulkan {

Buffer::Buffer(): context(nullptr), buffer(VK_NULL_HANDLE), allocation(VK_NULL_HANDLE), allocation_info() {}

Buffer::Buffer(Context* context, VkBuffer buffer, VmaAllocation allocation, VmaAllocationInfo allocation_info)
    : context(context), buffer(buffer), allocation(allocation), allocation_info(allocation_info) { }

void Buffer::update(const void* data, uint32_t size) {
    void* mapped;
    context->map_memory(allocation, &mapped);
    memcpy(mapped, data, size);
    context->unmap_memory(allocation);
}

VkBuffer Buffer::get_buffer() const {
    return buffer;
}

}