#include "image.hpp"

namespace Morpho::Vulkan {

Image::Image(): image(VK_NULL_HANDLE), allocation(VK_NULL_HANDLE), allocation_info({}) { }

Image::Image(VkImage image, VmaAllocation allocation, VmaAllocationInfo allocation_info)
    : image(image), allocation(allocation), allocation_info(allocation_info) { }

Image::Image(VkImage image): image(image), allocation(VK_NULL_HANDLE),allocation_info({}) { }

VkImage Image::get_image() const {
    return image;
}

VmaAllocation Image::get_allocation() const {
    return allocation;
}

}
